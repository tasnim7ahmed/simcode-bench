#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket.h" // For UDP socket type

#include <iostream>
#include <string>

// Define namespace for convenience
using namespace ns3;

// --- Custom UDP Server Application ---
class UdpServerApp : public Application
{
public:
    static TypeId GetTypeId();

    UdpServerApp();
    ~UdpServerApp() override;

    void Setup(uint16_t port);

private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

TypeId UdpServerApp::GetTypeId()
{
    static TypeId tid = TypeId("UdpServerApp")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpServerApp>();
    return tid;
}

UdpServerApp::UdpServerApp()
    : m_socket(nullptr),
      m_port(0)
{
}

UdpServerApp::~UdpServerApp()
{
    m_socket = nullptr; // Smart pointer will handle deallocation
}

void UdpServerApp::Setup(uint16_t port)
{
    m_port = port;
}

void UdpServerApp::StartApplication()
{
    if (m_socket == nullptr)
    {
        // Create a UDP socket
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        NS_ASSERT_MSG(m_socket != nullptr, "Failed to create UdpSocket");

        // Create an address to bind to
        InetSocketAddress localAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(localAddress) == -1)
        {
            NS_FATAL_ERROR("Failed to bind UDP socket to port " << m_port);
        }

        // Set the callback for received packets
        m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
        NS_LOG_INFO("UdpServerApp on node " << GetNode()->GetId() << " listening on port " << m_port);
    }
}

void UdpServerApp::StopApplication()
{
    if (m_socket != nullptr)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    NS_LOG_INFO("UdpServerApp on node " << GetNode()->GetId() << " stopped.");
}

void UdpServerApp::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address fromAddress;
    while ((packet = socket->RecvFrom(fromAddress)))
    {
        if (packet->GetSize() > 0)
        {
            InetSocketAddress senderInetAddress = InetSocketAddress::ConvertFrom(fromAddress);
            NS_LOG_INFO("Server received " << packet->GetSize() << " bytes from "
                                            << senderInetAddress.GetIpv4() << ":"
                                            << senderInetAddress.GetPort()
                                            << " at " << Simulator::Now().GetSeconds() << "s");
        }
    }
}

// --- Custom UDP Client Application ---
class UdpClientApp : public Application
{
public:
    static TypeId GetTypeId();

    UdpClientApp();
    ~UdpClientApp() override;

    void Setup(Ipv4Address peerAddress, uint16_t peerPort, uint32_t packetSize, Time sendInterval);

private:
    void StartApplication() override;
    void StopApplication() override;

    void SendPacket();

    Ptr<Socket> m_socket;
    Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    Time m_sendInterval;
    uint32_t m_packetsSent;
    EventId m_sendEvent;
};

TypeId UdpClientApp::GetTypeId()
{
    static TypeId tid = TypeId("UdpClientApp")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpClientApp>();
    return tid;
}

UdpClientApp::UdpClientApp()
    : m_socket(nullptr),
      m_peerAddress(),
      m_peerPort(0),
      m_packetSize(0),
      m_sendInterval(Time()),
      m_packetsSent(0),
      m_sendEvent()
{
}

UdpClientApp::~UdpClientApp()
{
    m_socket = nullptr;
}

void UdpClientApp::Setup(Ipv4Address peerAddress, uint16_t peerPort, uint32_t packetSize, Time sendInterval)
{
    m_peerAddress = peerAddress;
    m_peerPort = peerPort;
    m_packetSize = packetSize;
    m_sendInterval = sendInterval;
}

void UdpClientApp::StartApplication()
{
    // Create a UDP socket
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    NS_ASSERT_MSG(m_socket != nullptr, "Failed to create UdpSocket");

    NS_LOG_INFO("UdpClientApp on node " << GetNode()->GetId() << " starting to send to "
                                        << m_peerAddress << ":" << m_peerPort);
    m_packetsSent = 0;
    // Schedule the first packet
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpClientApp::SendPacket, this);
}

void UdpClientApp::StopApplication()
{
    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket != nullptr)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    NS_LOG_INFO("UdpClientApp on node " << GetNode()->GetId() << " stopped. Sent " << m_packetsSent << " packets.");
}

void UdpClientApp::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    // Create destination address
    InetSocketAddress remoteAddress(m_peerAddress, m_peerPort);

    int ret = m_socket->SendTo(packet, 0, remoteAddress);
    if (ret > 0)
    {
        m_packetsSent++;
        NS_LOG_INFO("Client sent " << packet->GetSize() << " bytes to "
                                    << m_peerAddress << ":" << m_peerPort
                                    << " (total " << m_packetsSent << ") at "
                                    << Simulator::Now().GetSeconds() << "s");
    }
    else
    {
        NS_LOG_WARN("Client failed to send packet: " << ret);
    }

    // Schedule the next packet if application is still running
    if (Simulator::Now() < GetStopTime())
    {
        m_sendEvent = Simulator::Schedule(m_sendInterval, &UdpClientApp::SendPacket, this);
    }
}

// --- Main Simulation Function ---
int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpServerApp", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApp", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_INFO); // Useful for debugging connectivity

    // Define simulation parameters
    uint32_t numStaNodes = 2; // One client, one server
    double simulationTime = 10.0; // seconds
    uint16_t serverPort = 9999;
    uint32_t packetSize = 1024; // bytes
    Time sendInterval = Seconds(1.0); // seconds

    // 1. Create nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(numStaNodes);

    NodeContainer allNodes;
    allNodes.Add(apNode);
    allNodes.Add(staNodes);

    // 2. Configure Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Set positions for visual clarity/to ensure they are within range
    apNode.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0)); // Client
    staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(-10.0, 0.0, 0.0)); // Server

    // 3. Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Modern Wi-Fi standard

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure MAC for AP and STA
    NqosWifiMacHelper apMac = NqosWifiMacHelper::Default();
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("my-wifi-network")));

    NqosWifiMacHelper staMac = NqosWifiMacHelper::Default();
    staMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("my-wifi-network")),
                   "ActiveProbing", BooleanValue(false)); // Active probing can be disabled for simplicity

    // Install Wi-Fi devices
    NetDeviceContainer apDevices = wifi.Install(phy, apMac, apNode);
    NetDeviceContainer staDevices = wifi.Install(phy, staMac, staNodes);

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Set up IP routing for AP (required for infrastructure mode)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Install Custom UDP Applications
    Ptr<Node> clientNode = staNodes.Get(0);
    Ptr<Node> serverNode = staNodes.Get(1);
    Ipv4Address serverIpAddress = staInterfaces.GetAddress(1); // Server is staNodes.Get(1)

    // Server application setup
    Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
    serverApp->SetStartTime(Seconds(1.0)); // Start server slightly before client
    serverApp->SetStopTime(Seconds(simulationTime + 0.5)); // Stop server slightly after client
    serverNode->AddApplication(serverApp);
    serverApp->Setup(serverPort);

    // Client application setup
    Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
    clientApp->SetStartTime(Seconds(1.5)); // Start client after server is ready
    clientApp->SetStopTime(Seconds(simulationTime));
    clientNode->AddApplication(clientApp);
    clientApp->Setup(serverIpAddress, serverPort, packetSize, sendInterval);

    // 7. Simulation Run
    Simulator::Stop(Seconds(simulationTime + 1.0)); // Ensure all applications have time to stop
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}