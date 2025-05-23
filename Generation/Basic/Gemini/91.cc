#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/udp-socket.h"

// Forward declarations for packet reception callbacks
void PacketReceivedAtSource (Ptr<Socket> socket, const Address &from, const Address &to, Ptr<Packet> packet);
void PacketReceivedAtDestination (Ptr<Socket> socket, const Address &from, const Address &to, Ptr<Packet> packet);

// NS_LOG_COMPONENT_DEFINE for logging output
NS_LOG_COMPONENT_DEFINE ("MultiInterfaceStaticRouting");

/**
 * \brief Custom application for the source node that dynamically binds UDP sockets
 * to different interfaces and sends packets to corresponding destination IPs.
 */
class DynamicallyBindingUdpSender : public Application
{
public:
    DynamicallyBindingUdpSender ();
    ~DynamicallyBindingUdpSender () override;

    /**
     * \brief Setup the sender application.
     *
     * \param socket1 The first UDP socket.
     * \param localAddress1 The local IP address for socket1.
     * \param destAddress1 The destination IP address for packets sent via socket1.
     * \param socket2 The second UDP socket.
     * \param localAddress2 The local IP address for socket2.
     * \param destAddress2 The destination IP address for packets sent via socket2.
     * \param port The UDP port to use.
     * \param packetSize The size of each packet in bytes.
     * \param numPackets The total number of packets to send.
     * \param dataRate The data rate at which to send packets.
     */
    void Setup (Ptr<Socket> socket1, Ipv4Address localAddress1, Ipv4Address destAddress1,
                Ptr<Socket> socket2, Ipv4Address localAddress2, Ipv4Address destAddress2,
                uint16_t port, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
    void StartApplication () override;
    void StopApplication () override;

    void SendPacket ();

    Ptr<Socket> m_socket1;        ///< First UDP socket
    Ipv4Address m_localAddress1;  ///< Local IP address for socket1
    Ipv4Address m_destAddress1;   ///< Destination IP address for socket1 packets
    uint16_t m_port;              ///< UDP port for both sockets

    Ptr<Socket> m_socket2;        ///< Second UDP socket
    Ipv4Address m_localAddress2;  ///< Local IP address for socket2
    Ipv4Address m_destAddress2;   ///< Destination IP address for socket2 packets

    uint32_t m_packetSize;        ///< Size of packets
    uint32_t m_numPackets;        ///< Total number of packets to send
    uint32_t m_packetsSent;       ///< Number of packets sent so far
    DataRate m_dataRate;          ///< Data rate for sending packets
    EventId m_sendEvent;          ///< Event for scheduling packet sends
    bool m_useSocket1Next;        ///< Flag to alternate between sockets for sending
};

DynamicallyBindingUdpSender::DynamicallyBindingUdpSender ()
    : m_socket1 (nullptr),
      m_socket2 (nullptr),
      m_packetSize (0),
      m_numPackets (0),
      m_packetsSent (0),
      m_useSocket1Next (true) // Start with socket1
{}

DynamicallyBindingUdpSender::~DynamicallyBindingUdpSender ()
{
    m_socket1 = nullptr;
    m_socket2 = nullptr;
}

void
DynamicallyBindingUdpSender::Setup (Ptr<Socket> socket1, Ipv4Address localAddress1, Ipv4Address destAddress1,
                                    Ptr<Socket> socket2, Ipv4Address localAddress2, Ipv4Address destAddress2,
                                    uint16_t port, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
    m_socket1 = socket1;
    m_localAddress1 = localAddress1;
    m_destAddress1 = destAddress1;
    m_port = port;

    m_socket2 = socket2;
    m_localAddress2 = localAddress2;
    m_destAddress2 = destAddress2;

    m_packetSize = packetSize;
    m_numPackets = numPackets;
    m_dataRate = dataRate;
}

void
DynamicallyBindingUdpSender::StartApplication ()
{
    m_packetsSent = 0;

    // Bind socket1 to localAddress1 and set receive callback
    m_socket1->Bind (InetSocketAddress (m_localAddress1, m_port));
    m_socket1->SetRecvCallback (MakeCallback (&PacketReceivedAtSource));

    // Bind socket2 to localAddress2 and set receive callback
    m_socket2->Bind (InetSocketAddress (m_localAddress2, m_port));
    m_socket2->SetRecvCallback (MakeCallback (&PacketReceivedAtSource));

    // Schedule the first packet send
    m_sendEvent = Simulator::Schedule (Seconds (0.0), &DynamicallyBindingUdpSender::SendPacket, this);
}

void
DynamicallyBindingUdpSender::StopApplication ()
{
    Simulator::Cancel (m_sendEvent);
    if (m_socket1)
    {
        m_socket1->Close ();
    }
    if (m_socket2)
    {
        m_socket2->Close ();
    }
}

void
DynamicallyBindingUdpSender::SendPacket ()
{
    if (m_packetsSent < m_numPackets)
    {
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        InetSocketAddress destAddr;
        InetSocketAddress localAddr; // For logging only, socket is already bound
        Ptr<Socket> currentSocket;

        if (m_useSocket1Next)
        {
            currentSocket = m_socket1;
            destAddr = InetSocketAddress (m_destAddress1, m_port);
            localAddr = InetSocketAddress (m_localAddress1, m_port);
            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s: Source " << localAddr.GetIpv4 ()
                                     << " sending " << m_packetSize << " bytes to " << destAddr.GetIpv4 ()
                                     << " (packet " << m_packetsSent + 1 << "/" << m_numPackets << ")");
        }
        else
        {
            currentSocket = m_socket2;
            destAddr = InetSocketAddress (m_destAddress2, m_port);
            localAddr = InetSocketAddress (m_localAddress2, m_port);
            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s: Source " << localAddr.GetIpv4 ()
                                     << " sending " << m_packetSize << " bytes to " << destAddr.GetIpv4 ()
                                     << " (packet " << m_packetsSent + 1 << "/" << m_numPackets << ")");
        }

        currentSocket->SendTo (packet, 0, destAddr);
        m_packetsSent++;
        m_useSocket1Next = !m_useSocket1Next; // Toggle for next send

        // Schedule next send based on data rate
        Time nextTxTime = NanoSeconds (m_packetSize * 8 / m_dataRate.GetBitsPerSecond ());
        m_sendEvent = Simulator::Schedule (nextTxTime, &DynamicallyBindingUdpSender::SendPacket, this);
    }
    else
    {
        StopApplication (); // All packets sent, stop the application
    }
}

/**
 * \brief Callback function for packet reception at the source node.
 * \param socket The socket that received the packet.
 * \param from The address of the sender.
 * \param to The local address (interface) where the packet was received.
 * \param packet The received packet.
 */
void
PacketReceivedAtSource (Ptr<Socket> socket, const Address &from, const Address &to, Ptr<Packet> packet)
{
    NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s: Source received "
                             << packet->GetSize () << " bytes from "
                             << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                             << " on local interface " << InetSocketAddress::ConvertFrom (to).GetIpv4 () << ".");
}

/**
 * \brief Callback function for packet reception at the destination node.
 * \param socket The socket that received the packet.
 * \param from The address of the sender.
 * \param to The local address (interface) where the packet was received.
 * \param packet The received packet.
 */
void
PacketReceivedAtDestination (Ptr<Socket> socket, const Address &from, const Address &to, Ptr<Packet> packet)
{
    NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s: DESTINATION received "
                             << packet->GetSize () << " bytes from "
                             << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                             << " on local interface " << InetSocketAddress::ConvertFrom (to).GetIpv4 () << ".");
    // Optionally, echo the packet back
    socket->SendTo (packet, 0, from);
}

int main (int argc, char *argv[])
{
    // Enable logging for relevant components
    LogComponentEnable ("MultiInterfaceStaticRouting", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpSocketImpl", LOG_LEVEL_WARN); // Show some socket warnings
    LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_INFO); // Log routing decisions
    LogComponentEnable ("Ipv4StaticRouting", LOG_LEVEL_INFO); // Log static routing table info

    // Command line arguments for simulation parameters
    CommandLine cmd (__FILE__);
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    DataRate dataRate ("1Mbps");
    double simTime = 10.0;
    uint16_t port = 9; // Discard port typically

    cmd.AddValue ("packetSize", "Size of packets in bytes", packetSize);
    cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue ("dataRate", "Data rate of packets", dataRate.ToString ());
    cmd.AddValue ("simTime", "Total simulation time", simTime);
    cmd.Parse (argc, argv);

    // Create nodes: Source (S), Router1 (R1), Router2 (R2), Destination (D)
    NodeContainer nodes;
    nodes.Create (4);
    Ptr<Node> sourceNode = nodes.Get (0);
    Ptr<Node> router1Node = nodes.Get (1);
    Ptr<Node> router2Node = nodes.Get (2);
    Ptr<Node> destinationNode = nodes.Get (3);

    // Install Internet stack on all nodes (includes Ipv4, Icmp, Ipv4StaticRouting, etc.)
    InternetStackHelper internet;
    internet.Install (nodes);

    // Configure Point-to-Point link characteristics
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices_SR1, devices_SR2, devices_R1D, devices_R2D;

    // Create links and assign IP addresses
    Ipv4AddressHelper ipv4;

    // S - R1 link (Network 10.1.1.0/24)
    devices_SR1 = p2p.Install (sourceNode, router1Node);
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_SR1 = ipv4.Assign (devices_SR1); // S: 10.1.1.1, R1: 10.1.1.2

    // S - R2 link (Network 10.1.2.0/24)
    devices_SR2 = p2p.Install (sourceNode, router2Node);
    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_SR2 = ipv4.Assign (devices_SR2); // S: 10.1.2.1, R2: 10.1.2.2

    // R1 - D link (Network 10.1.3.0/24)
    devices_R1D = p2p.Install (router1Node, destinationNode);
    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_R1D = ipv4.Assign (devices_R1D); // R1: 10.1.3.1, D: 10.1.3.2

    // R2 - D link (Network 10.1.4.0/24)
    devices_R2D = p2p.Install (router2Node, destinationNode);
    ipv4.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_R2D = ipv4.Assign (devices_R2D); // R2: 10.1.4.1, D: 10.1.4.2

    // Get Ipv4StaticRouting protocol for each node to configure routes
    Ptr<Ipv4StaticRouting> srcStaticRouting =
        DynamicCast<Ipv4StaticRouting> (sourceNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
    Ptr<Ipv4StaticRouting> r1StaticRouting =
        DynamicCast<Ipv4StaticRouting> (router1Node->GetObject<Ipv4> ()->GetRoutingProtocol ());
    Ptr<Ipv4StaticRouting> r2StaticRouting =
        DynamicCast<Ipv4StaticRouting> (router2Node->GetObject<Ipv4> ()->GetRoutingProtocol ());
    Ptr<Ipv4StaticRouting> destStaticRouting =
        DynamicCast<Ipv4StaticRouting> (destinationNode->GetObject<Ipv4> ()->GetRoutingProtocol ());

    // Configure static routes for all nodes
    // Note: ns-3's Ipv4Stack already sets up routes for directly connected networks.
    // Static routes are needed for non-directly connected networks.

    // Source (S) routes to Destination subnets
    // Route to D's 10.1.3.0/24 subnet via R1 (10.1.1.2), metric 1
    srcStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"),
                                         interfaces_SR1.GetAddress (1), 1);
    // Route to D's 10.1.4.0/24 subnet via R2 (10.1.2.2), metric 2
    srcStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"),
                                         interfaces_SR2.GetAddress (1), 2);
    // These metrics on the source node influence path selection IF the destination could be reached via multiple routes
    // through different next-hops for the same prefix. Here, the application chooses different destination IPs,
    // so the distinct prefixes ensure path separation.

    // Router 1 (R1) routes
    // R1 is connected to 10.1.1.0/24 (S) and 10.1.3.0/24 (D).
    // R1 needs routes to 10.1.2.0/24 (S's other net) and 10.1.4.0/24 (D's other net) if it were part of a larger network.
    // In this specific isolated star topology, it's not strictly necessary for packets to reach these
    // other subnets through R1. However, adding them here to fulfill general "static routing"
    // and demonstrate how they would be configured if R1 was a central router.
    // The next-hops for these routes would be incorrect if S and D aren't routers.
    // For this specific setup, return paths are symmetric (packet goes S->R1->D, reply goes D->R1->S).
    // If a different metric was needed on R1 to reach a specific destination also reachable via R2 (e.g. R1-R2 link), it would be added here.
    // Not explicitly adding placeholder routes here to avoid misleading or incorrect routing logic.

    // Router 2 (R2) routes (Similar logic to R1)
    // R2 is connected to 10.1.2.0/24 (S) and 10.1.4.0/24 (D).

    // Destination (D) routes to Source subnets
    // These metrics will influence return path choice if D sends to S's distinct IPs.
    // Return to S's 10.1.1.0/24 subnet via R1 (10.1.3.1), metric 1
    destStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"),
                                          interfaces_R1D.GetAddress (0), 1);
    // Return to S's 10.1.2.0/24 subnet via R2 (10.1.4.1), metric 2
    destStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"),
                                          interfaces_R2D.GetAddress (0), 2);

    // Install UDP Echo Servers on Destination Node
    // Server 1 listens on D's interface connected to R1 (10.1.3.2)
    UdpEchoServerHelper echoServer1 (port);
    ApplicationContainer serverApps1 = echoServer1.Install (destinationNode);
    serverApps1.Start (Seconds (0.0));
    serverApps1.Stop (Seconds (simTime + 1.0));
    // Hook Rx callback for server 1
    Config::ConnectWithoutContext ("/NodeList/3/ApplicationList/0/$ns3::UdpEchoServer/Rx",
                                   MakeCallback (&PacketReceivedAtDestination));

    // Server 2 listens on D's interface connected to R2 (10.1.4.2)
    UdpEchoServerHelper echoServer2 (port); // Uses the same port, relies on distinct destination IPs
    ApplicationContainer serverApps2 = echoServer2.Install (destinationNode);
    serverApps2.Start (Seconds (0.0));
    serverApps2.Stop (Seconds (simTime + 1.0));
    // Hook Rx callback for server 2
    Config::ConnectWithoutContext ("/NodeList/3/ApplicationList/1/$ns3::UdpEchoServer/Rx",
                                   MakeCallback (&PacketReceivedAtDestination));

    // Install custom UDP Sender on Source Node
    Ptr<Socket> ns3UdpSocket1 = Socket::CreateSocket (sourceNode, UdpSocketFactory::GetTypeId ());
    Ptr<Socket> ns3UdpSocket2 = Socket::CreateSocket (sourceNode, UdpSocketFactory::GetTypeId ());

    Ptr<DynamicallyBindingUdpSender> senderApp = CreateObject<DynamicallyBindingUdpSender> ();
    senderApp->Setup (ns3UdpSocket1, interfaces_SR1.GetAddress (0), interfaces_R1D.GetAddress (1), // Send from 10.1.1.1 to 10.1.3.2
                      ns3UdpSocket2, interfaces_SR2.GetAddress (0), interfaces_R2D.GetAddress (1), // Send from 10.1.2.1 to 10.1.4.2
                      port, packetSize, numPackets, dataRate);
    sourceNode->AddApplication (senderApp);
    senderApp->SetStartTime (Seconds (1.0)); // Start sending after 1 second
    senderApp->SetStopTime (Seconds (simTime)); // Stop at simulation end time

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll ("multi-interface-static-routing");

    // Set simulation stop time
    Simulator::Stop (Seconds (simTime + 2.0)); // Add some buffer for cleanup

    // Run the simulation
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}