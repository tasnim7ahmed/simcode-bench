#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpBroadcastExample");

// Custom UDP receiver application
class UdpPacketReceiver : public Application
{
public:
    UdpPacketReceiver() : m_socket(0) {}
    virtual ~UdpPacketReceiver() { m_socket = 0; }

    void Setup(Address address, uint16_t port)
    {
        m_address = address;
        m_port = port;
    }

private:
    virtual void StartApplication(void)
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&UdpPacketReceiver::HandleRead, this));
        }
    }

    virtual void StopApplication(void)
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s Node " << GetNode()->GetId() << " received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }

    Ptr<Socket> m_socket;
    Address m_address;
    uint16_t m_port;
};

// UDP broadcast sender application
class UdpBroadcastSender : public Application
{
public:
    UdpBroadcastSender() : m_socket(0) {}

    void Setup(Ipv4Address broadcastAddr, uint16_t port, uint32_t packetSize, double interval, uint32_t numPackets)
    {
        m_broadcastAddr = broadcastAddr;
        m_port = port;
        m_packetSize = packetSize;
        m_interval = Seconds(interval);
        m_numPackets = numPackets;
        m_sentPackets = 0;
    }

private:
    virtual void StartApplication()
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->SetAllowBroadcast(true);
        m_dest = InetSocketAddress(m_broadcastAddr, m_port);
        SendPacket();
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->SendTo(packet, 0, m_dest);
        ++m_sentPackets;
        if (m_sentPackets < m_numPackets)
        {
            Simulator::Schedule(m_interval, &UdpBroadcastSender::SendPacket, this);
        }
    }

    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    Ptr<Socket> m_socket;
    Ipv4Address m_broadcastAddr;
    InetSocketAddress m_dest;
    uint16_t m_port;
    uint32_t m_packetSize;
    Time m_interval;
    uint32_t m_numPackets;
    uint32_t m_sentPackets;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("WifiUdpBroadcastExample", LOG_LEVEL_INFO);

    uint32_t numReceivers = 4;
    uint32_t numNodes = numReceivers + 1; // 1 sender + receivers
    double simulationTime = 10.0; // seconds
    uint16_t port = 4000;
    uint32_t packetSize = 512;
    double interval = 1.0; // seconds
    uint32_t numPackets = 8;

    NodeContainer wifiNodes;
    wifiNodes.Create(numNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("simple-wifi");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // Set the sender node mac to AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    wifi.Install(phy, mac, wifiNodes.Get(0)); // first node as sender/AP

    // Use constant position mobility (no motion)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        positionAlloc->Add(Vector(5.0 * i, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    // AP was installed separately: assign it to node 0 explicitly.
    Ipv4InterfaceContainer apInterface;
    address.NewNetwork();
    NetDeviceContainer apDev;
    apDev.Add(wifiNodes.Get(0)->GetDevice(1)); // AP is always device index 1
    apInterface = address.Assign(apDev);

    // Retrieve IPv4 broadcast address
    Ipv4Address broadcastAddr("10.0.0.255");

    // Install receiver app on each receiver node
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        Ptr<UdpPacketReceiver> app = CreateObject<UdpPacketReceiver>();
        app->Setup(Ipv4Address::GetAny(), port);
        wifiNodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simulationTime));
    }

    // Install sender app on sender node (node 0)
    Ptr<UdpBroadcastSender> senderApp = CreateObject<UdpBroadcastSender>();
    senderApp->Setup(broadcastAddr, port, packetSize, interval, numPackets);
    wifiNodes.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(2.0));
    senderApp->SetStopTime(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}