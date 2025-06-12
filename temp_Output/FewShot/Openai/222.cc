#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpCustomApps");

class UdpServerApp : public Application
{
public:
    UdpServerApp() : m_socket(0) {}
    virtual ~UdpServerApp() { m_socket = 0; }

    void Setup(Address address, uint16_t port)
    {
        m_local = InetSocketAddress(Ipv4Address::GetAny(), port);
    }
protected:
    virtual void StartApplication() override
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        }
        m_socket->Bind(m_local);
        m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            uint32_t recvSize = packet->GetSize();
            InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
            NS_LOG_INFO("At " << Simulator::Now().GetSeconds() << "s, Server received " << recvSize
                         << " bytes from " << addr.GetIpv4());
        }
    }

private:
    Ptr<Socket> m_socket;
    Address m_local;
};

class UdpClientApp : public Application
{
public:
    UdpClientApp()
        : m_socket(0), m_peer(), m_packetSize(1024), m_nPackets(0), m_interval(Seconds(1.0)), m_sent(0) {}

    virtual ~UdpClientApp() { m_socket = 0; }

    void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
    {
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_interval = interval;
    }

protected:
    virtual void StartApplication() override
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(m_peer);
        m_sent = 0;
        SendPacket();
    }

    virtual void StopApplication() override
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

        ++m_sent;
        if (m_sent < m_nPackets)
        {
            Simulator::Schedule(m_interval, &UdpClientApp::SendPacket, this);
        }
    }

private:
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_sent;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("WifiUdpCustomApps", LOG_LEVEL_INFO);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiServerNode;
    wifiServerNode.Create(1);

    // Configure Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Station (client) MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNodes);

    // Access Point MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Server connects to AP via wired connection (CSMA)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NodeContainer apServer;
    apServer.Add(wifiApNode.Get(0));
    apServer.Add(wifiServerNode.Get(0));
    NetDeviceContainer csmaDevices = csma.Install(apServer);

    // Mobility for stations and AP
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);
    mobility.Install(wifiServerNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);
    stack.Install(wifiServerNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);
    address.Assign(apDevice);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // Set up UDP Server on server node
    uint16_t port = 4000;
    Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
    serverApp->Setup(csmaInterfaces.GetAddress(1), port);
    wifiServerNode.Get(0)->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(1.0));
    serverApp->SetStopTime(Seconds(10.0));

    // Set up UDP Client on station node
    Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
    // Client targets the server's IP (csmaInterfaces.GetAddress(1)) and port
    clientApp->Setup(InetSocketAddress(csmaInterfaces.GetAddress(1), port),
                     1024,              // Packet size
                     9,                 // Number of packets (1 per second for 9s)
                     Seconds(1.0));     // Interval
    wifiStaNodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(2.0));
    clientApp->SetStopTime(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}