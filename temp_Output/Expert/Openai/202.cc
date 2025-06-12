#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnCircularTopology");

static uint32_t packetsSent = 0;
static uint32_t packetsReceived = 0;
static double totalDelay = 0.0;

void RxCallback(Ptr<const Packet> packet, const Address &address, double rxTime, double txTime)
{
    ++packetsReceived;
    totalDelay += (rxTime - txTime);
}

class CustomUdpClient : public Application
{
public:
    CustomUdpClient() : m_socket(0), m_peer(), m_packetSize(50), m_nPackets(0), m_sendEvent(), m_interval(Seconds(1.0)), m_packetsSent(0) {}
    virtual ~CustomUdpClient() { m_socket = 0; }

    void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
    {
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_interval = interval;
    }

private:
    virtual void StartApplication(void)
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_socket->Connect(m_peer);
        }
        m_packetsSent = 0;
        SendPacket();
    }

    virtual void StopApplication(void)
    {
        if (m_socket)
        {
            m_socket->Close();
        }
        Simulator::Cancel(m_sendEvent);
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        Time txTime = Simulator::Now();
        packet->AddPacketTag(TxTimeTag(txTime.GetSeconds()));
        m_socket->Send(packet);
        ++packetsSent;
        ++m_packetsSent;
        if (m_packetsSent < m_nPackets)
        {
            m_sendEvent = Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
        }
    }

    class TxTimeTag : public Tag
    {
    public:
        TxTimeTag() : m_time(0.0) {}
        TxTimeTag(double t) : m_time(t) {}
        static TypeId GetTypeId(void)
        {
            static TypeId tid = TypeId("TxTimeTag")
                .SetParent<Tag>()
                .AddConstructor<TxTimeTag>();
            return tid;
        }
        virtual TypeId GetInstanceTypeId(void) const
        {
            return GetTypeId();
        }
        virtual void Serialize(TagBuffer i) const
        {
            i.WriteDouble(m_time);
        }
        virtual void Deserialize(TagBuffer i)
        {
            m_time = i.ReadDouble();
        }
        virtual uint32_t GetSerializedSize(void) const
        {
            return 8;
        }
        virtual void Print(std::ostream &os) const
        {
            os << m_time;
        }
        double GetTime() const { return m_time; }
    private:
        double m_time;
    };

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    EventId m_sendEvent;
    Time m_interval;
    uint32_t m_packetsSent;
};

void SinkRx(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        double rxTime = Simulator::Now().GetSeconds();
        CustomUdpClient::TxTimeTag tag;
        if (packet->PeekPacketTag(tag))
        {
            double txTime = tag.GetTime();
            RxCallback(packet, from, rxTime, txTime);
        }
    }
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double radius = 20.0;
    double simulationTime = 30.0;
    uint32_t packetSize = 50;
    uint32_t packetsPerSource = 20;
    double interval = 1.0; // seconds

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);
    lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        double angle = 2 * M_PI * i / numNodes;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaces = ipv6.Assign(sixlowpanDevices);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        ifaces.SetForwarding(i, true);
        ifaces.SetDefaultRouteInAllNodes(i);
    }

    uint16_t port = 5683;
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    Inet6SocketAddress localAddr = Inet6SocketAddress(ifaces.GetAddress(0, 1), port);
    sinkSocket->Bind(localAddr);
    sinkSocket->SetRecvCallback(MakeCallback(&SinkRx));

    for (uint32_t i = 1; i < numNodes; ++i)
    {
        Ptr<CustomUdpClient> app = CreateObject<CustomUdpClient>();
        Inet6SocketAddress remoteAddr = Inet6SocketAddress(ifaces.GetAddress(0, 1), port);
        remoteAddr.SetTos(0x70);
        app->Setup(remoteAddr, packetSize, packetsPerSource, Seconds(interval));
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(i * 0.5 + 1.5)); // stagger start times
        app->SetStopTime(Seconds(simulationTime - 1));
    }

    AnimationInterface anim("wsn-circular.xml");
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), i == 0 ? "Sink" : "Sensor");
        anim.UpdateNodeColor(nodes.Get(i), i == 0 ? 255 : 0, i == 0 ? 0 : 255, 0);
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier>(flowmonHelper.GetClassifier6());
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();

    uint32_t totalTx = 0, totalRx = 0;
    double totalThroughput = 0, totalE2eDelay = 0;
    uint32_t flows = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if (t.destinationAddress == ifaces.GetAddress(0, 1))
        {
            ++flows;
            totalTx += it->second.txPackets;
            totalRx += it->second.rxPackets;
            if (it->second.rxPackets > 0)
                totalE2eDelay += it->second.delaySum.GetSeconds();
            if (it->second.timeLastRxPacket.GetSeconds() > 0 && it->second.timeFirstTxPacket.GetSeconds() > 0)
                totalThroughput += (double)it->second.rxBytes * 8.0 /
                    (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1024;
        }
    }

    double pdr = (totalTx > 0) ? ((double)totalRx / (double)totalTx) : 0.0;
    double e2eDelay = (totalRx > 0) ? (totalE2eDelay / totalRx) : 0.0;
    double avgThroughput = totalThroughput / flows;

    std::cout << "Packet Delivery Ratio: " << pdr * 100 << "%" << std::endl;
    std::cout << "End-to-End Delay: " << e2eDelay * 1000 << " ms" << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}