#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0;

void
TxCallback(Ptr<const Packet> packet)
{
    packetsSent++;
}

void
RxCallback(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        packetsReceived++;
        // Extract and calculate delay if the packet has timestamp
        Time sendTime;
        if (packet->PeekPacketTag(sendTime))
        {
            Time delay = Simulator::Now() - sendTime;
            totalDelay += delay.GetSeconds();
        }
    }
}

class TimestampTag : public Tag
{
public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("TimestampTag")
            .SetParent<Tag>()
            .AddConstructor<TimestampTag>();
        return tid;
    }

    virtual TypeId GetInstanceTypeId() const override
    {
        return GetTypeId();
    }
    virtual uint32_t GetSerializedSize() const override
    {
        return 8;
    }
    virtual void Serialize(TagBuffer i) const override
    {
        int64_t t = m_time.GetNanoSeconds();
        i.Write((const uint8_t*)&t, 8);
    }
    virtual void Deserialize(TagBuffer i) override
    {
        int64_t t;
        i.Read((uint8_t*)&t, 8);
        m_time = NanoSeconds(t);
    }
    virtual void Print(std::ostream& os) const override
    {
        os << m_time.GetSeconds();
    }
    void SetTimestamp(Time time)
    {
        m_time = time;
    }
    Time GetTimestamp() const
    {
        return m_time;
    }
private:
    Time m_time;
};

void
AppSendPacket(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t dstPort, uint32_t packetSize, uint32_t nPackets, Time pktInterval)
{
    Ptr<Packet> packet = Create<Packet>(packetSize);

    TimestampTag tag;
    tag.SetTimestamp(Simulator::Now());
    packet->AddPacketTag(tag);

    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, dstPort));
    TxCallback(packet);

    if (--nPackets > 0)
    {
        Simulator::Schedule(pktInterval, &AppSendPacket, socket, dstAddr, dstPort, packetSize, nPackets, pktInterval);
    }
}

void
ReceivePacket(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        packetsReceived++;

        TimestampTag tag;
        if (packet->PeekPacketTag(tag))
        {
            Time sendTime = tag.GetTimestamp();
            Time delay = Simulator::Now() - sendTime;
            totalDelay += delay.GetSeconds();
        }
    }
}

void
TrackMobility(Ptr<Node> node)
{
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    Vector pos = mob->GetPosition();
    std::cout << "Time " << Simulator::Now().GetSeconds()
              << "s: Node " << node->GetId()
              << " Position (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    Simulator::Schedule(Seconds(1.0), &TrackMobility, node);
}

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("ManetSimulation", LOG_LEVEL_INFO);

    uint32_t numNodes = 3;
    double simTime = 20.0;
    uint16_t port = 4000;

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    mobility.Install(nodes);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint32_t packetSize = 64;
    uint32_t numPackets = 30;
    Time interPacketInterval = Seconds(1.0);

    std::vector<Ptr<Socket>> sourceSockets;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        recvSocket->Bind(local);
        recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    }

    // Each node sends to the next one (loop)
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t dstIdx = (i + 1) % numNodes;
        Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        sourceSockets.push_back(srcSocket);

        Simulator::ScheduleWithContext(
            srcSocket->GetNode()->GetId(),
            Seconds(2.0), // give time for routing convergence
            &AppSendPacket,
            srcSocket,
            interfaces.GetAddress(dstIdx),
            port,
            packetSize,
            numPackets,
            interPacketInterval
        );
    }

    // Schedule mobility tracking
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Simulator::Schedule(Seconds(0.0), &TrackMobility, nodes.Get(i));
    }

    AnimationInterface anim("manet.xml");
    anim.SetMaxPktsPerTraceFile(200000);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    double pdr = (packetsSent == 0) ? 0.0 : ((double)packetsReceived / packetsSent) * 100;
    double avgDelay = (packetsReceived == 0) ? 0.0 : totalDelay / packetsReceived;

    std::cout << "Sent: " << packetsSent << std::endl;
    std::cout << "Received: " << packetsReceived << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
    std::cout << "Average Latency: " << avgDelay << "s" << std::endl;

    return 0;
}