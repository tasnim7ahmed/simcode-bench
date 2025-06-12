#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiUdpSocketExample");

class UdpServerApp : public Application
{
public:
    UdpServerApp() : m_socket(0), m_port(4000) {}
    virtual ~UdpServerApp() { m_socket = 0; }

    void Setup(uint16_t port)
    {
        m_port = port;
    }

private:
    virtual void StartApplication()
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            if (m_socket->Bind(local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
        }
        m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
    }

    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Address from;
        while (Ptr<Packet> packet = socket->RecvFrom(from))
        {
            NS_LOG_INFO("Server received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

class UdpClientApp : public Application
{
public:
    UdpClientApp()
        : m_socket(0), m_peerAddress(), m_peerPort(0), m_packetSize(1024), m_nPackets(0),
          m_interval(Seconds(1.0)), m_sent(0) {}

    virtual ~UdpClientApp() { m_socket = 0; }

    void Setup(Address address, uint16_t port, uint32_t packetSize, Time interval)
    {
        m_peerAddress = address;
        m_peerPort = port;
        m_packetSize = packetSize;
        m_interval = interval;
    }

private:
    virtual void StartApplication()
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));
        }
        m_sent = 0;
        SendPacket();
    }

    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);
        m_sent++;
        NS_LOG_INFO("Client sent packet " << m_sent);

        if (Simulator::Now().GetSeconds() + m_interval.GetSeconds() < 10.01)
        {
            Simulator::Schedule(m_interval, &UdpClientApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_sent;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("AdhocWifiUdpSocketExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create and install UdpServerApp on node 1
    Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
    serverApp->Setup(4000);
    nodes.Get(1)->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(1.0));
    serverApp->SetStopTime(Seconds(10.0));

    // Create and install UdpClientApp on node 0
    Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
    clientApp->Setup(interfaces.GetAddress(1), 4000, 512, Seconds(1.0));
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(2.0));
    clientApp->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}