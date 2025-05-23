#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/callback.h"
#include "ns3/ipv4-global-routing-helper.h" // For routing tables

// Define a log component for the custom relay application
NS_LOG_COMPONENT_DEFINE("UdpRelayApplication");

/*
 * Custom UDP Relay Application:
 * This application listens for UDP packets on a specified port.
 * When a packet is received, it logs the event and then forwards the packet
 * to a pre-configured destination (the actual UDP server).
 * It acts as an application-level proxy.
 */
class UdpRelayApplication : public ns3::Application
{
public:
    UdpRelayApplication();
    ~UdpRelayApplication() override;

    // Configure the relay with the destination server's address and port
    void Setup(ns3::Ipv4Address serverAddress, uint16_t serverPort);

private:
    void StartApplication() override;
    void StopApplication() override;

    // Callback for handling received packets on the listening socket
    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    // Method to send the received packet to the backend server
    void SendPacketToBackend(ns3::Ptr<ns3::Packet> packet);

    ns3::Ptr<ns3::Socket> m_recvSocket; // Socket to receive packets from the client
    uint16_t m_recvPort;                // Port to listen on for client packets

    ns3::Ptr<ns3::Socket> m_sendSocket; // Socket to send packets to the actual server
    ns3::Ipv4Address m_serverAddress;   // IP address of the actual UDP server
    uint16_t m_serverPort;              // Port of the actual UDP server
};

UdpRelayApplication::UdpRelayApplication()
    : m_recvPort(0), m_serverPort(0)
{
    NS_LOG_FUNCTION(this);
}

UdpRelayApplication::~UdpRelayApplication()
{
    NS_LOG_FUNCTION(this);
}

void UdpRelayApplication::Setup(ns3::Ipv4Address serverAddress, uint16_t serverPort)
{
    NS_LOG_FUNCTION(this << serverAddress << serverPort);
    m_serverAddress = serverAddress;
    m_serverPort = serverPort;
    m_recvPort = 8888; // Hardcode the port the relay listens on for client traffic
}

void UdpRelayApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    // Create and bind the socket for receiving packets from the client
    if (!m_recvSocket)
    {
        m_recvSocket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
        ns3::InetSocketAddress local = ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), m_recvPort);
        if (m_recvSocket->Bind(local) == -1)
        {
            NS_LOG_ERROR("Failed to bind UDP relay receive socket to " << local);
            return;
        }
    }
    m_recvSocket->SetRecvCallback(MakeCallback(&UdpRelayApplication::HandleRead, this));

    // Create and connect the socket for sending packets to the backend server
    if (!m_sendSocket)
    {
        m_sendSocket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
        ns3::InetSocketAddress remote = ns3::InetSocketAddress(m_serverAddress, m_serverPort);
        if (m_sendSocket->Connect(remote) == -1)
        {
            NS_LOG_ERROR("Failed to connect UDP relay send socket to " << remote);
            return;
        }
    }

    NS_LOG_INFO("UDP Relay on Node " << GetNode()->GetId() << " listening on port " << m_recvPort << ", forwarding to " << m_serverAddress << ":" << m_serverPort);
}

void UdpRelayApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_recvSocket)
    {
        m_recvSocket->Close();
        m_recvSocket = nullptr;
    }
    if (m_sendSocket)
    {
        m_sendSocket->Close();
        m_sendSocket = nullptr;
    }
}

void UdpRelayApplication::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    ns3::Ptr<ns3::Packet> packet;
    ns3::InetSocketAddress from;
    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }
        NS_LOG_INFO("Relay: Received " << packet->GetSize() << " bytes from " << from.GetIpv4() << " on Node " << GetNode()->GetId());

        // Forward the packet to the actual server
        SendPacketToBackend(packet);
    }
}

void UdpRelayApplication::SendPacketToBackend(ns3::Ptr<ns3::Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    if (m_sendSocket && m_sendSocket->GetNode())
    {
        int ret = m_sendSocket->Send(packet);
        if (ret == -1)
        {
            NS_LOG_WARN("Relay: Failed to send packet to backend server: " << m_sendSocket->GetError());
        }
        else
        {
            NS_LOG_INFO("Relay: Sent " << ret << " bytes to backend server " << m_serverAddress << ":" << m_serverPort << " from Node " << GetNode()->GetId());
        }
    }
    else
    {
        NS_LOG_WARN("Relay: Send socket not ready for sending to backend.");
    }
}

// Define a log component for the main simulation script
NS_LOG_COMPONENT_DEFINE("CsmaUdpRelaySimulation");

// Callback function for the UDP server to log received packets
static void UdpServerRxCallback(ns3::Ptr<const ns3::Packet> packet,
                                const ns3::Address& address,
                                const ns3::Address& localAddress)
{
    ns3::InetSocketAddress sender = ns3::InetSocketAddress::ConvertFrom(address);
    ns3::InetSocketAddress receiver = ns3::InetSocketAddress::ConvertFrom(localAddress);
    NS_LOG_INFO("Server: Received " << packet->GetSize() << " bytes from " << sender.GetIpv4() << " on local port " << receiver.GetPort());
}

int main(int argc, char* argv[])
{
    // Enable logging for relevant components
    ns3::LogComponentEnable("CsmaUdpRelaySimulation", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpRelayApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    // Allow command-line arguments for simulation control (e.g., logging)
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("log", "Enable specific log components (e.g., CsmaUdpRelaySimulation=level)");
    cmd.Parse(argc, argv);

    // 1. Create Nodes
    NS_LOG_INFO("Creating nodes...");
    ns3::NodeContainer nodes;
    nodes.Create(3); // Node 0: Client, Node 1: Relay, Node 2: Server

    // 2. Create CSMA NetDevice and connect nodes
    NS_LOG_INFO("Creating CSMA network...");
    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    ns3::NetDeviceContainer devices = csma.Install(nodes);

    // 3. Install Internet Stack on all nodes
    NS_LOG_INFO("Installing Internet stack...");
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP Addresses to the CSMA devices
    NS_LOG_INFO("Assigning IP addresses...");
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Populate routing tables. This is generally good practice in NS-3
    // for IP-enabled nodes, even if direct communication on a single
    // segment doesn't strictly depend on it for basic forwarding.
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 5. Configure UDP Server (Node 2)
    NS_LOG_INFO("Setting up UDP Server on Node 2...");
    uint16_t serverPort = 9999;
    ns3::UdpEchoServerHelper echoServer(serverPort);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Connect the Rx trace source of the server application to our callback
    ns3::Ptr<ns3::UdpEchoServerApplication> serverApp = ns3::DynamicCast<ns3::UdpEchoServerApplication>(serverApps.Get(0));
    serverApp->TraceConnectWithoutContext("Rx", ns3::MakeCallback(&UdpServerRxCallback));

    // 6. Configure Custom UDP Relay (Node 1)
    NS_LOG_INFO("Setting up custom UDP Relay on Node 1...");
    ns3::Ptr<UdpRelayApplication> relayApp = ns3::CreateObject<UdpRelayApplication>();
    // Relay needs to know the destination server's IP and port
    relayApp->Setup(interfaces.GetAddress(2), serverPort); 
    nodes.Get(1)->AddApplication(relayApp);
    relayApp->Start(ns3::Seconds(1.5)); // Start after server
    relayApp->Stop(ns3::Seconds(10.5)); // Stop before end of simulation

    // 7. Configure UDP Client (Node 0)
    NS_LOG_INFO("Setting up UDP Client on Node 0...");
    uint16_t relayListenPort = 8888; // The port the relay is configured to listen on
    // Client sends packets to the Relay's IP address and its listening port
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(1), relayListenPort); 
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start after server and relay
    clientApps.Stop(ns3::Seconds(10.0));

    // 8. Run Simulation
    NS_LOG_INFO("Running simulation...");
    ns3::Simulator::Stop(ns3::Seconds(11.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}