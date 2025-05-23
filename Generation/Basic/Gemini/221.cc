#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"

#include <iostream>
#include <string>

NS_LOG_COMPONENT_DEFINE("UdpClientApp");
NS_LOG_COMPONENT_DEFINE("UdpServerApp");

class UdpClientApp : public ns3::Application
{
public:
    UdpClientApp();
    ~UdpClientApp() override;

    void Setup(ns3::Ipv4Address addr1, uint16_t port1, ns3::Ipv4Address addr2, uint16_t port2,
               uint32_t packetSize, ns3::Time sendInterval);

private:
    void StartApplication() override;
    void StopApplication() override;

    void SendPacket();

    ns3::Ptr<ns3::Socket> m_socket;
    ns3::Ipv4Address m_peerAddress1;
    uint16_t m_peerPort1;
    ns3::Ipv4Address m_peerAddress2;
    uint16_t m_peerPort2;
    uint32_t m_packetSize;
    ns3::Time m_sendInterval;
    uint32_t m_packetsSent;
    bool m_sendToFirstServer;
};

UdpClientApp::UdpClientApp()
    : m_socket(nullptr),
      m_peerAddress1(),
      m_peerPort1(0),
      m_peerAddress2(),
      m_peerPort2(0),
      m_packetSize(0),
      m_sendInterval(ns3::Time::S(0)),
      m_packetsSent(0),
      m_sendToFirstServer(true)
{
}

UdpClientApp::~UdpClientApp()
{
}

void UdpClientApp::Setup(ns3::Ipv4Address addr1, uint16_t port1, ns3::Ipv4Address addr2, uint16_t port2,
                         uint32_t packetSize, ns3::Time sendInterval)
{
    m_peerAddress1 = addr1;
    m_peerPort1 = port1;
    m_peerAddress2 = addr2;
    m_peerPort2 = port2;
    m_packetSize = packetSize;
    m_sendInterval = sendInterval;
}

void UdpClientApp::StartApplication()
{
    NS_LOG_FUNCTION(this);
    m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
    m_socket->Bind();

    ns3::Simulator::Schedule(ns3::Time::S(0.0), &UdpClientApp::SendPacket, this);
}

void UdpClientApp::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void UdpClientApp::SendPacket()
{
    NS_LOG_FUNCTION(this);

    ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(m_packetSize);

    if (m_sendToFirstServer)
    {
        m_socket->SendTo(packet, 0, ns3::InetSocketAddress(m_peerAddress1, m_peerPort1));
        NS_LOG_INFO("Client " << GetNode()->GetId() << " sending " << m_packetSize << " bytes to "
                               << m_peerAddress1 << ":" << m_peerPort1);
    }
    else
    {
        m_socket->SendTo(packet, 0, ns3::InetSocketAddress(m_peerAddress2, m_peerPort2));
        NS_LOG_INFO("Client " << GetNode()->GetId() << " sending " << m_packetSize << " bytes to "
                               << m_peerAddress2 << ":" << m_peerPort2);
    }

    m_packetsSent++;
    m_sendToFirstServer = !m_sendToFirstServer;

    if (ns3::Simulator::Now() < ns3::Time::S(10.0))
    {
        ns3::Simulator::Schedule(m_sendInterval, &UdpClientApp::SendPacket, this);
    }
}

class UdpServerApp : public ns3::Application
{
public:
    UdpServerApp();
    ~UdpServerApp() override;

    void Setup(uint16_t port);

private:
    void StartApplication() override;
    void StopApplication() override;
    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    ns3::Ptr<ns3::Socket> m_socket;
    uint16_t m_localPort;
    uint32_t m_packetsReceived;
};

UdpServerApp::UdpServerApp()
    : m_socket(nullptr),
      m_localPort(0),
      m_packetsReceived(0)
{
}

UdpServerApp::~UdpServerApp()
{
}

void UdpServerApp::Setup(uint16_t port)
{
    m_localPort = port;
}

void UdpServerApp::StartApplication()
{
    NS_LOG_FUNCTION(this);
    m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
    ns3::InetSocketAddress local = ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), m_localPort);
    if (m_socket->Bind(local) == -1)
    {
        NS_FATAL_ERROR("Failed to bind socket to port " << m_localPort);
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
}

void UdpServerApp::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void UdpServerApp::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    NS_LOG_FUNCTION(this);
    ns3::Ptr<ns3::Packet> packet;
    ns3::Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        if (ns3::InetSocketAddress::Is(from))
        {
            ns3::InetSocketAddress addr = ns3::InetSocketAddress::ConvertFrom(from);
            NS_LOG_INFO("Server " << GetNode()->GetId() << " (port " << m_localPort << ") received "
                                   << packet->GetSize() << " bytes from "
                                   << addr.GetIpv4() << ":" << addr.GetPort()
                                   << " at " << ns3::Simulator::Now().GetSeconds() << "s");
            m_packetsReceived++;
        }
    }
}

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("UdpClientApp", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpServerApp", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpSocketImpl", ns3::LOG_LEVEL_INFO);

    uint32_t packetSize = 1024;
    ns3::Time sendInterval = ns3::MilliSeconds(500);
    uint16_t serverPort1 = 9000;
    uint16_t serverPort2 = 9001;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of packets to send (bytes)", packetSize);
    cmd.AddValue("sendInterval", "Time interval between packet sends (ms)", sendInterval.GetMilliSeconds());
    cmd.AddValue("serverPort1", "UDP port for server 1", serverPort1);
    cmd.AddValue("serverPort2", "UDP port for server 2", serverPort2);
    cmd.Parse(argc, argv);

    sendInterval = ns3::MilliSeconds(sendInterval.GetMilliSeconds());

    ns3::NodeContainer nodes;
    nodes.Create(4); // Client, Switch, Server1, Server2
    ns3::Ptr<ns3::Node> clientNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> switchNode = nodes.Get(1);
    ns3::Ptr<ns3::Node> server1Node = nodes.Get(2);
    ns3::Ptr<ns3::Node> server2Node = nodes.Get(3);

    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer clientSwitchDevices = p2pHelper.Install(clientNode, switchNode);
    ns3::NetDeviceContainer switchServer1Devices = p2pHelper.Install(switchNode, server1Node);
    ns3::NetDeviceContainer switchServer2Devices = p2pHelper.Install(switchNode, server2Node);

    ns3::InternetStackHelper internet;
    internet.Install(clientNode);
    internet.Install(server1Node);
    internet.Install(server2Node);

    ns3::BridgeHelper bridge;
    bridge.Install(switchNode,
                   ns3::NetDeviceContainer(clientSwitchDevices.Get(1),
                                           switchServer1Devices.Get(0),
                                           switchServer2Devices.Get(0)));

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    ns3::Ipv4InterfaceContainer clientInterface = ipv4.Assign(clientSwitchDevices.Get(0));
    ns3::Ipv4InterfaceContainer server1Interface = ipv4.Assign(switchServer1Devices.Get(1));
    ns3::Ipv4InterfaceContainer server2Interface = ipv4.Assign(switchServer2Devices.Get(1));

    ns3::Ptr<UdpServerApp> server1App = ns3::CreateObject<UdpServerApp>();
    server1App->SetStartTime(ns3::Time::S(0.0));
    server1App->SetStopTime(ns3::Time::S(10.0));
    server1Node->AddApplication(server1App);
    server1App->Setup(serverPort1);

    ns3::Ptr<UdpServerApp> server2App = ns3::CreateObject<UdpServerApp>();
    server2App->SetStartTime(ns3::Time::S(0.0));
    server2App->SetStopTime(ns3::Time::S(10.0));
    server2Node->AddApplication(server2App);
    server2App->Setup(serverPort2);

    ns3::Ptr<UdpClientApp> clientApp = ns3::CreateObject<UdpClientApp>();
    clientApp->SetStartTime(ns3::Time::S(0.0));
    clientApp->SetStopTime(ns3::Time::S(10.0));
    clientNode->AddApplication(clientApp);
    clientApp->Setup(server1Interface.GetAddress(0), serverPort1,
                      server2Interface.GetAddress(0), serverPort2,
                      packetSize, sendInterval);

    ns3::Simulator::Stop(ns3::Time::S(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}