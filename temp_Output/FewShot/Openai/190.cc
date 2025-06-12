#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

// Store statistics
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void
TxTrace(Ptr<const Packet> packet)
{
    packetsSent++;
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    packetsReceived++;
}

void
RxWithDelay(Ptr<const Packet> packet, const Address &address, double sendTime)
{
    packetsReceived++;
    double now = Simulator::Now().GetSeconds();
    totalDelay += (now - sendTime);
}

class UdpClientWithTimestamp : public Application
{
public:
    UdpClientWithTimestamp();
    virtual ~UdpClientWithTimestamp();

    void Setup(Address address, uint16_t port, uint32_t pktSize, uint32_t nPackets, double interval);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_pktSize;
    uint32_t m_nPackets;
    uint32_t m_sent;
    double m_interval;
    EventId m_sendEvent;
};

UdpClientWithTimestamp::UdpClientWithTimestamp()
    : m_socket(0), m_peerPort(0), m_pktSize(0), m_nPackets(0), m_sent(0), m_interval(1.0)
{
}

UdpClientWithTimestamp::~UdpClientWithTimestamp()
{
    m_socket = 0;
}

void UdpClientWithTimestamp::Setup(Address address, uint16_t port, uint32_t pktSize, uint32_t nPackets, double interval)
{
    m_peerAddress = address;
    m_peerPort = port;
    m_pktSize = pktSize;
    m_nPackets = nPackets;
    m_interval = interval;
}

void UdpClientWithTimestamp::StartApplication()
{
    m_sent = 0;
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));
    SendPacket();
}

void UdpClientWithTimestamp::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
    }
    Simulator::Cancel(m_sendEvent);
}

void UdpClientWithTimestamp::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    uint64_t time = Simulator::Now().GetNanoSeconds();
    packet->AddPacketTag(TimestampTag(time));
    m_socket->Send(packet);
    packetsSent++;
    m_sent++;
    if (m_sent < m_nPackets)
    {
        m_sendEvent = Simulator::Schedule(Seconds(m_interval), &UdpClientWithTimestamp::SendPacket, this);
    }
}

// Custom TimestampTag for measuring delays
class TimestampTag : public Tag
{
public:
    TimestampTag() : m_timestamp(0) {}
    TimestampTag(uint64_t ts) : m_timestamp(ts) {}

    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("TimestampTag")
            .SetParent<Tag>()
            .AddConstructor<TimestampTag>();
        return tid;
    }

    TypeId GetInstanceTypeId(void) const override
    {
        return GetTypeId();
    }

    void Serialize(TagBuffer i) const override
    {
        i.WriteU64(m_timestamp);
    }

    void Deserialize(TagBuffer i) override
    {
        m_timestamp = i.ReadU64();
    }

    uint32_t GetSerializedSize() const override
    {
        return sizeof(uint64_t);
    }

    void Print(std::ostream &os) const override
    {
        os << "t=" << m_timestamp;
    }

    uint64_t GetTimestamp() const
    {
        return m_timestamp;
    }

private:
    uint64_t m_timestamp;
};

void ReceivePacketWithTimestamp(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        packetsReceived++;
        TimestampTag tag;
        packet->PeekPacketTag(tag);
        uint64_t sendTs = tag.GetTimestamp();
        double delay = (Simulator::Now().GetNanoSeconds() - sendTs) / 1e9;
        totalDelay += delay;
    }
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 3;
    double simTime = 30.0;
    uint32_t packetSize = 512;
    uint32_t packetsPerClient = 20;
    double appStart = 2.0;
    double appStop = 28.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Logging
    LogComponentEnable("UdpClientWithTimestamp", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wifi settings
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility: RandomDirection2dMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=90.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=90.0]"));
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on node 2
    uint16_t port = 50000;
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    sinkSocket->Bind(local);
    sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacketWithTimestamp));

    // UDP client app on node 0
    Ptr<UdpClientWithTimestamp> clientApp1 = CreateObject<UdpClientWithTimestamp>();
    clientApp1->Setup(interfaces.GetAddress(2), port, packetSize, packetsPerClient, 1.0);
    nodes.Get(0)->AddApplication(clientApp1);
    clientApp1->SetStartTime(Seconds(appStart));
    clientApp1->SetStopTime(Seconds(appStop));

    // UDP client app on node 1
    Ptr<UdpClientWithTimestamp> clientApp2 = CreateObject<UdpClientWithTimestamp>();
    clientApp2->Setup(interfaces.GetAddress(2), port, packetSize, packetsPerClient, 1.5);
    nodes.Get(1)->AddApplication(clientApp2);
    clientApp2->SetStartTime(Seconds(appStart + 0.5));
    clientApp2->SetStopTime(Seconds(appStop));

    // NetAnim: assign IDs and enable node update
    AnimationInterface anim("manet-netanim.xml");
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), "Node"+std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0); // All red
    }
    anim.EnablePacketMetadata(true);
    anim.EnableWifiMacCounters(Seconds(0), Seconds(simTime));
    anim.EnableWifiPhyCounters(Seconds(0), Seconds(simTime));

    // Set constant random seed for repeatability
    RngSeedManager::SetSeed(12345);

    // Track mobility
    Config::Connect("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback(
        [](Ptr<const MobilityModel> mobility) {
            Vector pos = mobility->GetPosition();
            NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << "s: Node at " << pos.x << ", " << pos.y);
        }
    ));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Output metrics
    std::cout << "Packets sent: " << packetsSent << std::endl;
    std::cout << "Packets received: " << packetsReceived << std::endl;
    double pdr = (packetsSent > 0) ? ((double)packetsReceived / (double)packetsSent) : 0.0;
    std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
    double avgDelay = (packetsReceived > 0) ? (totalDelay / packetsReceived) : 0.0;
    std::cout << "Average Latency: " << avgDelay << " seconds" << std::endl;

    Simulator::Destroy();
    return 0;
}