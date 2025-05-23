#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

// Define a custom application for Node 2 to forward UDP packets
class UdpForwarder : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    UdpForwarder();
    virtual ~UdpForwarder();
    void SetTxParameters(ns3::Ipv4Address peerAddress, uint16_t peerPort);
    void SetRxParameters(uint16_t localPort);

protected:
    virtual void DoInitialize();
    virtual void DoDispose();
    virtual void StartApplication();
    virtual void StopApplication();

private:
    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    ns3::Ptr<ns3::Socket> m_socketRx;     // Socket for receiving
    ns3::Ptr<ns3::Socket> m_socketTx;     // Socket for sending
    ns3::Ipv4Address m_peerAddress;       // Destination address for forwarded packets (Node 3)
    uint16_t m_peerPort;                  // Destination port for forwarded packets (Node 3)
    uint16_t m_localPort;                 // Local port to listen on (Node 2)
};

ns3::TypeId UdpForwarder::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("ns3::UdpForwarder")
        .SetParent<ns3::Application>()
        .SetGroupName("Applications")
        .AddConstructor<UdpForwarder>()
        .AddAttribute("PeerAddress",
                      "The destination IP address for the forwarded packets.",
                      ns3::Ipv4AddressValue(),
                      ns3::MakeIpv4AddressAccessor(&UdpForwarder::m_peerAddress),
                      ns3::MakeIpv4AddressChecker())
        .AddAttribute("PeerPort",
                      "The destination port for the forwarded packets.",
                      ns3::UintegerValue(0),
                      ns3::MakeUintegerAccessor(&UdpForwarder::m_peerPort),
                      ns3::MakeUintegerChecker<uint16_t>())
        .AddAttribute("LocalPort",
                      "The local port to listen on for incoming packets.",
                      ns3::UintegerValue(0),
                      ns3::MakeUintegerAccessor(&UdpForwarder::m_localPort),
                      ns3::MakeUintegerChecker<uint16_t>());
    return tid;
}

UdpForwarder::UdpForwarder()
    : m_socketRx(nullptr),
      m_socketTx(nullptr),
      m_peerAddress(),
      m_peerPort(0),
      m_localPort(0)
{
}

UdpForwarder::~UdpForwarder()
{
}

void UdpForwarder::DoInitialize()
{
    ns3::Application::DoInitialize();
}

void UdpForwarder::DoDispose()
{
    m_socketRx = nullptr;
    m_socketTx = nullptr;
    ns3::Application::DoDispose();
}

void UdpForwarder::SetTxParameters(ns3::Ipv4Address peerAddress, uint16_t peerPort)
{
    m_peerAddress = peerAddress;
    m_peerPort = peerPort;
}

void UdpForwarder::SetRxParameters(uint16_t localPort)
{
    m_localPort = localPort;
}

void UdpForwarder::StartApplication()
{
    // Create socket for receiving from Node 1
    if (!m_socketRx)
    {
        m_socketRx = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
        ns3::InetSocketAddress local = ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), m_localPort);
        if (m_socketRx->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind UdpForwarder Rx socket");
        }
        m_socketRx->SetRecvCallback(ns3::MakeCallback(&UdpForwarder::HandleRead, this));
    }

    // Create socket for sending to Node 3
    if (!m_socketTx)
    {
        m_socketTx = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
        m_socketTx->Connect(ns3::InetSocketAddress(m_peerAddress, m_peerPort));
    }
}

void UdpForwarder::StopApplication()
{
    if (m_socketRx)
    {
        m_socketRx->Close();
    }
    if (m_socketTx)
    {
        m_socketTx->Close();
    }
}

void UdpForwarder::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    ns3::Ptr<ns3::Packet> packet;
    ns3::Address fromAddress;
    while ((packet = socket->RecvFrom(fromAddress)))
    {
        // Log incoming packet details
        if (ns3::InetSocketAddress::Is (fromAddress))
        {
            ns3::InetSocketAddress remoteAddress = ns3::InetSocketAddress::ConvertFrom(fromAddress);
            NS_LOG_INFO("UdpForwarder received " << packet->GetSize() << " bytes from "
                                                << remoteAddress.GetIpv4() << ":" << remoteAddress.GetPort()
                                                << " at " << ns3::Simulator::Now().GetSeconds() << "s. Forwarding...");
        }
        else
        {
            NS_LOG_INFO("UdpForwarder received " << packet->GetSize() << " bytes from unknown address type at "
                                                << ns3::Simulator::Now().GetSeconds() << "s. Forwarding...");
        }
        
        // Send the received packet to the peer address and port using the Tx socket
        m_socketTx->Send(packet); 
        NS_LOG_INFO("UdpForwarder forwarded " << packet->GetSize() << " bytes to "
                                            << m_peerAddress << ":" << m_peerPort
                                            << " at " << ns3::Simulator::Now().GetSeconds() << "s.");
    }
}

// Main simulation function
int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    ns3::LogComponentEnable("UdpForwarder", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);

    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 1. Create Nodes: one AP, two stations
    ns3::NodeContainer nodes;
    nodes.Create(3);
    ns3::Ptr<ns3::Node> apNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> staNode1 = nodes.Get(1); // Node 1 (station) sends to AP
    ns3::Ptr<ns3::Node> staNode2 = nodes.Get(2); // Node 3 (station) receives from AP

    // 2. Setup Wi-Fi parameters
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n); // Using 802.11n for modern setup

    // Set AARF (Automatic Rate Fallback) station management for both AP and STA
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    ns3::WifiMacHelper mac;
    ns3::NetDeviceContainer apDevice;
    ns3::NetDeviceContainer staDevices;

    // Configure AP
    mac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue("my-ssid"));
    apDevice = wifi.Install(phy, mac, apNode);

    // Configure Stations
    mac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue("my-ssid"));
    staDevices = wifi.Install(phy, mac, ns3::NodeContainer(staNode1, staNode2));

    // 3. Mobility: Place nodes at fixed positions
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    apNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    staNode1->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(10.0, 0.0, 0.0));
    staNode2->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 10.0, 0.0));

    // 4. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apIpInterface = address.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staIpInterfaces = address.Assign(staDevices);

    // Populate routing tables (important for IP communication between nodes)
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Applications
    uint16_t sinkPortNode3 = 10;   // Port for Node 3 (PacketSink)
    uint16_t forwarderPortNode2 = 9; // Port for Node 2 (UdpForwarder) to listen on
    uint32_t packetSize = 1024; // Packet size for UdpClient

    // Node 3 (Station 2) acts as the final server (PacketSink)
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                           ns3::InetSocketAddress(staIpInterfaces.GetAddress(1), sinkPortNode3)); 
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(staNode2);
    sinkApps.Start(ns3::Seconds(1.0)); // Server starts at 1s
    sinkApps.Stop(ns3::Seconds(10.0));

    // Node 2 (AP) acts as the UdpForwarder
    ns3::Ipv4Address forwarderRxAddress = apIpInterface.GetAddress(0); // AP's IP address
    ns3::Ipv4Address forwarderTxPeerAddress = staIpInterfaces.GetAddress(1); // Node 3's IP address

    ns3::ObjectFactory forwarderFactory;
    forwarderFactory.SetTypeId("ns3::UdpForwarder");
    forwarderFactory.Set("LocalPort", ns3::UintegerValue(forwarderPortNode2));
    forwarderFactory.Set("PeerAddress", ns3::Ipv4AddressValue(forwarderTxPeerAddress));
    forwarderFactory.Set("PeerPort", ns3::UintegerValue(sinkPortNode3));
    ns3::Ptr<UdpForwarder> forwarderApp = forwarderFactory.Create<UdpForwarder>();
    apNode->AddApplication(forwarderApp);
    forwarderApp->SetStartTime(ns3::Seconds(1.0)); // Forwarder starts at 1s
    forwarderApp->SetStopTime(ns3::Seconds(10.0));

    // Node 1 (Station 1) acts as the UdpClient
    ns3::UdpClientHelper udpClient(forwarderRxAddress, forwarderPortNode2);
    udpClient.SetAttribute("MaxPackets", ns3::UintegerValue(1000)); // Send up to 1000 packets
    udpClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // Send one packet every 0.1 seconds
    udpClient.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = udpClient.Install(staNode1);
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2s
    clientApps.Stop(ns3::Seconds(10.0));

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}