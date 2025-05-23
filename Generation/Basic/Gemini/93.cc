#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-info-tag.h" // For getting TOS/TTL from received packet

// ns-3 logging component for this simulation
NS_LOG_COMPONENT_DEFINE("UdpTosTtlSimulation");

// Global counter for the sender, passed by reference to allow modification across scheduled events
static uint32_t g_packetsSent = 0;

/**
 * \brief Function to send UDP packets from the source node.
 * \param socket The UDP socket to send from.
 * \param packetSize The size of each packet in bytes.
 * \param numPackets The total number of packets to send.
 * \param interval The time interval between sending packets.
 */
void SendPacket(ns3::Ptr<ns3::Socket> socket, uint32_t packetSize, uint32_t numPackets, ns3::Time interval)
{
    if (g_packetsSent < numPackets)
    {
        ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(packetSize);
        socket->Send(packet);
        NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s sender node "
                               << socket->GetNode()->GetId() << " sent packet " << g_packetsSent
                               << " of size " << packetSize << " bytes.");
        g_packetsSent++;
        ns3::Simulator::Schedule(interval, &SendPacket, socket, packetSize, numPackets, interval);
    }
    else
    {
        socket->Close();
        NS_LOG_INFO("Sender finished sending " << g_packetsSent << " packets. Closing socket.");
    }
}

/**
 * \brief Function to receive UDP packets at the destination node.
 * \param socket The UDP socket that received the packet.
 */
void ReceivePacket(ns3::Ptr<ns3::Socket> socket)
{
    ns3::Ptr<ns3::Packet> packet;
    ns3::Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        ns3::InetSocketAddress fromAddr = ns3::InetSocketAddress::ConvertFrom(from);
        NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s receiver node "
                               << socket->GetNode()->GetId() << " received packet from "
                               << fromAddr.GetIpv4() << " on port " << fromAddr.GetPort()
                               << " of size " << packet->GetSize() << " bytes.");

        ns3::PacketInfoTag packetInfo;
        if (packet->PeekPacketInfoTag(packetInfo))
        {
            uint8_t receivedTos = packetInfo.GetTos();
            uint8_t receivedTtl = packetInfo.GetTtl();
            NS_LOG_INFO("  Received TOS: " << (uint32_t)receivedTos << ", TTL: " << (uint32_t)receivedTtl);
        }
        else
        {
            NS_LOG_INFO("  No PacketInfoTag found (SO_RCV_TOS/TTL might not be enabled or no IP header).");
        }
    }
}

int main(int argc, char* argv[])
{
    // Enable logging for our custom component and related modules
    ns3::LogComponentEnable("UdpTosTtlSimulation", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpSocketImpl", ns3::LOG_LEVEL_INFO);

    // Command-line parameters with default values
    uint32_t packetSize = 1024;   // Size of each packet in bytes
    uint32_t numPackets = 10;     // Total number of packets to send
    double interval = 1.0;        // Time interval between sending packets (seconds)
    uint8_t tos = 0;              // IP_TOS value (0-255) for outgoing packets
    uint8_t ttl = 64;             // IP_TTL value (1-255) for outgoing packets
    bool recvTos = false;         // Enable SO_RCV_TOS on receiver socket to log TOS
    bool recvTtl = false;         // Enable SO_RCV_TTL on receiver socket to log TTL

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of packets in bytes.", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send.", numPackets);
    cmd.AddValue("interval", "Time interval between packets (seconds).", interval);
    cmd.AddValue("tos", "IP_TOS (Type of Service) value (0-255) for sender.", tos);
    cmd.AddValue("ttl", "IP_TTL (Time To Live) value (1-255) for sender.", ttl);
    cmd.AddValue("recvTos", "Enable SO_RCV_TOS on receiver socket (bool).", recvTos);
    cmd.AddValue("recvTtl", "Enable SO_RCV_TTL on receiver socket (bool).", recvTtl);
    cmd.Parse(argc, argv);

    // Validate parameters
    if (ttl == 0)
    {
        NS_LOG_ERROR("IP_TTL must be greater than 0.");
        return 1;
    }
    if (packetSize == 0)
    {
255.0.
        NS_LOG_ERROR("Packet size must be greater than 0.");
        return 1;
    }
    if (interval <= 0)
    {
        NS_LOG_ERROR("Packet interval must be positive.");
        return 1;
    }
    if (numPackets == 0)
    {
        NS_LOG_WARN("Number of packets is 0, no packets will be sent.");
    }

    // Set simulation time resolution to nanoseconds
    ns3::Time::SetResolution(ns3::Time::NS);

    // 1. Create two nodes for the LAN topology
    ns3::NodeContainer nodes;
    nodes.Create(2); // n0 and n1

    // 2. Install CSMA devices to connect the nodes
    ns3::CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csmaHelper.SetChannelAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(6560))); // Example delay
    ns3::NetDeviceContainer devices = csmaHelper.Install(nodes);

    // 3. Install Internet stack on all nodes (IPv4, ARP, etc.)
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Assign IP addresses to the devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // Network 10.1.1.0/24
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get IP address of node n1 (the receiver)
    ns3::Ipv4Address receiverIp = interfaces.GetAddress(1); // n1 is at index 1 in NodeContainer

    // 5. Setup Receiver (n1) - manual socket creation for fine-grained control
    uint16_t port = 9; // Destination port for UDP communication
    ns3::Ptr<ns3::Socket> receiverSocket =
        ns3::Socket::CreateSocket(nodes.Get(1), ns3::UdpSocketFactory::GetTypeId());
    receiverSocket->Bind(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port)); // Listen on any IP, specified port
    receiverSocket->SetRecvCallback(ns3::MakeCallback(&ReceivePacket)); // Set the receive callback

    // Enable SO_RCV_TOS and SO_RCV_TTL if requested by command-line arguments
    if (recvTos)
    {
        receiverSocket->SetRecvTos(true);
        NS_LOG_INFO("Receiver SO_RCV_TOS enabled to log incoming TOS value.");
    }
    if (recvTtl)
    {
        receiverSocket->SetRecvTtl(true);
        NS_LOG_INFO("Receiver SO_RCV_TTL enabled to log incoming TTL value.");
    }

    // 6. Setup Sender (n0) - manual socket creation for fine-grained control
    ns3::Ptr<ns3::Socket> senderSocket =
        ns3::Socket::CreateSocket(nodes.Get(0), ns3::UdpSocketFactory::GetTypeId());
    
    // Set IP_TOS and IP_TTL socket options for the sender
    senderSocket->SetIpTos(tos);
    senderSocket->SetIpTtl(ttl);
    
    senderSocket->Bind(); // Bind to an ephemeral local port
    senderSocket->Connect(ns3::InetSocketAddress(receiverIp, port)); // Connect to the receiver's IP and port

    // Schedule the first packet send.
    // Start sending after 1 second to allow the network configuration to settle.
    ns3::Simulator::Schedule(ns3::Seconds(1.0), &SendPacket, senderSocket, packetSize, numPackets, ns3::Seconds(interval));

    // Calculate simulation stop time:
    // 1.0s (start delay) + (numPackets * interval) + 2.0s (buffer for last packet to arrive and be processed)
    double stopTime = 1.0 + (static_cast<double>(numPackets) * interval) + 2.0;
    if (numPackets == 0) // If no packets are sent, still run for a minimum time
    {
        stopTime = 2.0;
    }
    ns3::Simulator::Stop(ns3::Seconds(stopTime));

    // 7. Run the simulation
    NS_LOG_INFO("Starting simulation for " << stopTime << " seconds...");
    ns3::Simulator::Run();
    NS_LOG_INFO("Simulation finished.");

    // 8. Cleanup simulation resources
    ns3::Simulator::Destroy();

    return 0;
}