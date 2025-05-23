#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket.h"
#include "ns3/ipv6-header.h"
#include "ns3/packet.h"

// NS_LOG_COMPONENT_DEFINE for receiver and sender functions
NS_LOG_COMPONENT_DEFINE("CsmaIPv6SocketOptionsReceiver");
NS_LOG_COMPONENT_DEFINE("CsmaIPv6SocketOptionsSender");

// Global variables for command-line arguments
uint32_t g_packetSize = 1000;
uint32_t g_numPackets = 10;
double g_interval = 1.0;
int32_t g_tclass = -1;    // Default to -1, meaning not set
int32_t g_hoplimit = -1;  // Default to -1, meaning not set
uint16_t g_port = 9;      // Echo port

/**
 * \brief Receiver callback function to log incoming packets and their IPv6 attributes.
 * \param socket The UDP socket receiving the packet.
 * \param packet The received packet.
 * \param from The source address of the packet.
 * \param rxAttributes The socket receive attributes, containing TCLASS and HOPLIMIT if enabled.
 */
void RxPacket(Ptr<Socket> socket, Ptr<Packet> packet, const Address& from, Ptr<const SocketRxAttributes> rxAttributes)
{
    NS_LOG_FUNCTION(socket << packet->GetSize() << from);

    Inet6SocketAddress senderAddress = Inet6SocketAddress::ConvertFrom(from);
    uint8_t tclass = 0;
    uint8_t hoplimit = 0;

    if (rxAttributes)
    {
        if (rxAttributes->GetIpRecvTrafficClassFlag())
        {
            tclass = rxAttributes->GetIpRecvTrafficClass();
        }
        if (rxAttributes->GetIpRecvHopLimitFlag())
        {
            hoplimit = rxAttributes->GetIpRecvHopLimit();
        }
    }

    NS_LOG_INFO("CsmaIPv6SocketOptionsReceiver", "Received " << packet->GetSize() << " bytes from "
                                 << senderAddress.GetIpv6() << ":" << senderAddress.GetPort()
                                 << ", TCLASS: " << (uint32_t)tclass
                                 << ", HOPLIMIT: " << (uint32_t)hoplimit);
}

/**
 * \brief Sender function to send UDP packets.
 * \param socket The UDP socket to send from.
 * \param dstAddr The destination IPv6 address.
 * \param port The destination port.
 * \param packetSize The size of each packet.
 * \param numPackets The total number of packets to send.
 * \param interval The interval between sending packets.
 */
static uint32_t s_packetsSent = 0;
void SendPacket(Ptr<Socket> socket, Ipv6Address dstAddr, uint16_t port, uint32_t packetSize, uint32_t numPackets, double interval)
{
    NS_LOG_FUNCTION(socket << dstAddr << port << packetSize << numPackets << interval);

    if (s_packetsSent < numPackets)
    {
        Ptr<Packet> packet = Create<Packet>(packetSize);
        socket->SendTo(packet, 0, Inet6SocketAddress(dstAddr, port));
        s_packetsSent++;
        NS_LOG_INFO("CsmaIPv6SocketOptionsSender", "Sent " << packet->GetSize() << " bytes to "
                                 << dstAddr << ":" << port
                                 << " (Packet " << s_packetsSent << "/" << numPackets << ")");
        Simulator::Schedule(Seconds(interval), &SendPacket, socket, dstAddr, port, packetSize, numPackets, interval);
    }
    else
    {
        NS_LOG_INFO("CsmaIPv6SocketOptionsSender", "Finished sending " << s_packetsSent << " packets.");
        socket->Close(); // Close the socket after all packets are sent
    }
}

int main(int argc, char *argv[])
{
    // Configure command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of packets in bytes", g_packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", g_numPackets);
    cmd.AddValue("interval", "Time interval between packets in seconds", g_interval);
    cmd.AddValue("tclass", "IPv6 Traffic Class (0-255), -1 for default (no set)", g_tclass);
    cmd.AddValue("hoplimit", "IPv6 Hop Limit (0-255), -1 for default (no set)", g_hoplimit);
    cmd.Parse(argc, argv);

    // Set logging level for components
    LogComponentEnable("CsmaIPv6SocketOptionsReceiver", LOG_LEVEL_INFO);
    LogComponentEnable("CsmaIPv6SocketOptionsSender", LOG_LEVEL_INFO);

    // Create two nodes (n0 and n1)
    NodeContainer nodes;
    nodes.Create(2); // n0 and n1

    // Install CSMA devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer devices = csma.Install(nodes);

    // Install IPv6 Internet stack on both nodes
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    // Assign IPv6 addresses to the CSMA interfaces
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Get IPv6 addresses for n0 and n1
    // The first address is the link-local, the second is the global unicast assigned by SetBase
    Ipv6Address n0GlobalAddr = interfaces.GetAddress(0, 1); // Address on node 0, interface 0
    Ipv6Address n1GlobalAddr = interfaces.GetAddress(1, 1); // Address on node 1, interface 0

    // Set up receiver (n1)
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Socket> sinkSocket = Socket::CreateSocket(n1, UdpSocketFactory::GetTypeId());
    Inet6SocketAddress sinkLocalAddress(Ipv6Address::GetAny(), g_port);
    sinkSocket->Bind(sinkLocalAddress);

    // Enable IPV6_RECVTCLASS and IPV6_RECVHOPLIMIT socket options on the receiver
    // These options cause the TCLASS and HOPLIMIT to be stored in SocketRxAttributes
    // and passed to the RxPacket callback.
    sinkSocket->SetIpRecvTrafficClass(true);
    sinkSocket->SetIpRecvHopLimit(true);

    // Set the receive callback for the sink socket
    sinkSocket->SetRecvCallback(MakeCallback(&RxPacket));

    // Set up sender (n0)
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Socket> clientSocket = Socket::CreateSocket(n0, UdpSocketFactory::GetTypeId());
    Inet6SocketAddress clientLocalAddress(Ipv6Address::GetAny(), 0); // Bind to any available port
    clientSocket->Bind(clientLocalAddress);

    // Set IPv6 TCLASS and HOPLIMIT socket options on the sender if specified via command-line
    if (g_tclass != -1)
    {
        clientSocket->SetIpTrafficClass(static_cast<uint8_t>(g_tclass));
        NS_LOG_INFO("CsmaIPv6SocketOptionsSender", "Set IPv6 Traffic Class on sender socket to " << g_tclass);
    }
    if (g_hoplimit != -1)
    {
        clientSocket->SetIpHopLimit(static_cast<uint8_t>(g_hoplimit));
        NS_LOG_INFO("CsmaIPv6SocketOptionsSender", "Set IPv6 Hop Limit on sender socket to " << g_hoplimit);
    }

    // Schedule the first packet send
    // Start sending after 1 second to allow nodes to configure and neighbor discovery to happen
    Simulator::Schedule(Seconds(1.0), &SendPacket, clientSocket, n1GlobalAddr, g_port, g_packetSize, g_numPackets, g_interval);

    // Calculate simulation stop time: 1s start delay + (numPackets - 1) * interval + some buffer
    double stopTime = 1.0 + (g_numPackets > 0 ? (g_numPackets - 1) * g_interval : 0) + 1.0;
    Simulator::Stop(Seconds(stopTime));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}