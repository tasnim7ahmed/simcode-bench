#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

NS_LOG_COMPONENT_DEFINE("UdpAdhocSimulation");

class UdpServerApp : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    UdpServerApp(uint16_t port);

protected:
    void DoStart() override;
    void DoDispose() override;

private:
    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    uint16_t m_port;
    ns3::Ptr<ns3::Socket> m_socket;
    uint32_t m_packetsReceived;
};

ns3::TypeId UdpServerApp::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("UdpServerApp")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("UdpAdhocSimulation")
                                 .AddConstructor<UdpServerApp>()
                                 .AddAttribute("Port",
                                               "Port on which to listen for incoming packets.",
                                               ns3::UintegerValue(9),
                                               ns3::MakeUintegerAccessor(&UdpServerApp::m_port),
                                               ns3::MakeUintegerChecker<uint16_t>());
    return tid;
}

UdpServerApp::UdpServerApp(uint16_t port)
    : m_port(port), m_socket(nullptr), m_packetsReceived(0)
{
    NS_LOG_FUNCTION(this << port);
}

void UdpServerApp::DoStart()
{
    NS_LOG_FUNCTION(this);

    m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
    NS_ASSERT_MSG(m_socket, "Failed to create socket");

    ns3::InetSocketAddress localAddress(ns3::Ipv4Address::GetAny(), m_port);
    int ret = m_socket->Bind(localAddress);
    if (ret == -1)
    {
        NS_FATAL_ERROR("Failed to bind socket to address " << localAddress << " on node " << GetNode()->GetId());
    }

    m_socket->SetRecvCallback(ns3::MakeCallback(&UdpServerApp::HandleRead, this));
    NS_LOG_INFO("UDP Server listening on port " << m_port << " on node " << GetNode()->GetId());
}

void UdpServerApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    ns3::Application::DoDispose();
}

void UdpServerApp::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    ns3::Ptr<ns3::Packet> packet;
    ns3::Address fromAddress;
    while ((packet = socket->RecvFrom(fromAddress)))
    {
        if (packet->GetSize() > 0)
        {
            ns3::InetSocketAddress from = ns3::InetSocketAddress::ConvertFrom(fromAddress);
            m_packetsReceived++;
            NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s: Server received "
                                   << packet->GetSize() << " bytes from "
                                   << from.GetIpv4() << " on node " << GetNode()->GetId()
                                   << ", Total received: " << m_packetsReceived << " packets.");
        }
    }
}

class UdpClientApp : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    UdpClientApp(ns3::Address peerAddress, uint16_t peerPort, uint32_t packetSize, ns3::Time interval);

protected:
    void DoStart() override;
    void DoDispose() override;

private:
    void SendPacket();

    ns3::Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    ns3::Time m_interval;
    ns3::Ptr<ns3::Socket> m_socket;
    uint32_t m_packetsSent;
    ns3::EventId m_sendEvent;
};

ns3::TypeId UdpClientApp::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("UdpClientApp")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("UdpAdhocSimulation")
                                 .AddConstructor<UdpClientApp>()
                                 .AddAttribute("PeerAddress",
                                               "The destination address of the UDP client.",
                                               ns3::AddressValue(),
                                               ns3::MakeAddressAccessor(&UdpClientApp::m_peerAddress),
                                               ns3::MakeAddressChecker())
                                 .AddAttribute("PeerPort",
                                               "The destination port of the UDP client.",
                                               ns3::UintegerValue(9),
                                               ns3::MakeUintegerAccessor(&UdpClientApp::m_peerPort),
                                               ns3::MakeUintegerChecker<uint16_t>())
                                 .AddAttribute("PacketSize",
                                               "The size of packets sent by the client.",
                                               ns3::UintegerValue(1024),
                                               ns3::MakeUintegerAccessor(&UdpClientApp::m_packetSize),
                                               ns3::MakeUintegerChecker<uint32_t>())
                                 .AddAttribute("Interval",
                                               "The time interval between packets sent by the client.",
                                               ns3::TimeValue(ns3::Seconds(1)),
                                               ns3::MakeTimeAccessor(&UdpClientApp::m_interval),
                                               ns3::MakeTimeChecker());
    return tid;
}

UdpClientApp::UdpClientApp(ns3::Address peerAddress,
                           uint16_t peerPort,
                           uint32_t packetSize,
                           ns3::Time interval)
    : m_peerAddress(peerAddress),
      m_peerPort(peerPort),
      m_packetSize(packetSize),
      m_interval(interval),
      m_socket(nullptr),
      m_packetsSent(0)
{
    NS_LOG_FUNCTION(this << peerAddress << peerPort << packetSize << interval.GetSeconds());
}

void UdpClientApp::DoStart()
{
    NS_LOG_FUNCTION(this);

    m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
    NS_ASSERT_MSG(m_socket, "Failed to create socket");

    m_socket->Connect(ns3::InetSocketAddress(ns3::Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));

    m_sendEvent = ns3::Simulator::Schedule(ns3::Seconds(0.0), &UdpClientApp::SendPacket, this);
    NS_LOG_INFO("UDP Client started on node " << GetNode()->GetId() << ", sending to "
                                              << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4()
                                              << ":" << m_peerPort);
}

void UdpClientApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_sendEvent.IsRunning())
    {
        ns3::Simulator::Cancel(m_sendEvent);
    }
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    ns3::Application::DoDispose();
}

void UdpClientApp::SendPacket()
{
    NS_LOG_FUNCTION(this);

    ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(m_packetSize);
    m_socket->Send(packet);
    m_packetsSent++;

    NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s: Client sent "
                           << m_packetSize << " bytes to "
                           << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4()
                           << " on node " << GetNode()->GetId()
                           << ", Total sent: " << m_packetsSent << " packets.");

    if (ns3::Simulator::Now() + m_interval < ns3::Simulator::GetStopTime())
    {
        m_sendEvent = ns3::Simulator::Schedule(m_interval, &UdpClientApp::SendPacket, this);
    }
}

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("UdpAdhocSimulation", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("WifiPhy", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("AdhocWifiMac", ns3::LOG_LEVEL_INFO);

    double simulationTime = 10.0;
    uint32_t packetSize = 1024;
    double sendInterval = 1.0;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("sendInterval", "Time interval between UDP packets in seconds", sendInterval);
    cmd.Parse(argc, argv);

    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n);

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(ns3::YansWifiChannelHelper::Create());

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    ns3::NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    ns3::MobilityHelper mobility;
    ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = ns3::CreateObject<ns3::ListPositionAllocator>();
    positionAlloc->Add(ns3::Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(ns3::Vector(1.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    ns3::Ptr<ns3::Node> serverNode = nodes.Get(1);
    uint16_t serverPort = 9;

    ns3::ApplicationContainer serverApps;
    ns3::ObjectFactory serverFactory;
    serverFactory.SetTypeId("UdpServerApp");
    serverFactory.Set("Port", ns3::UintegerValue(serverPort));
    serverApps.Add(serverFactory.Create<UdpServerApp>());
    serverNode->AddApplication(serverApps.Get(0));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime + 0.1));

    ns3::Ptr<ns3::Node> clientNode = nodes.Get(0);
    ns3::Ipv4Address serverIp = interfaces.GetAddress(1);

    ns3::ApplicationContainer clientApps;
    ns3::ObjectFactory clientFactory;
    clientFactory.SetTypeId("UdpClientApp");
    clientFactory.Set("PeerAddress", ns3::AddressValue(ns3::InetSocketAddress(serverIp, serverPort)));
    clientFactory.Set("PacketSize", ns3::UintegerValue(packetSize));
    clientFactory.Set("Interval", ns3::TimeValue(ns3::Seconds(sendInterval)));
    clientApps.Add(clientFactory.Create<UdpClientApp>());
    clientNode->AddApplication(clientApps.Get(0));
    clientApps.Start(ns3::Seconds(0.0));
    clientApps.Stop(ns3::Seconds(simulationTime + 0.1));

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}