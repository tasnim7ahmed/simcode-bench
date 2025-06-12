#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/data-collector.h"
#include "ns3/packet.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistanceExperiment");

// Statistics trackers
struct WifiStats
{
    uint32_t txPackets = 0;
    uint32_t rxPackets = 0;
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
    double   totDelay = 0.0;
};

WifiStats stats;

// Transmission callback
void
TxCallback(Ptr<const Packet> packet)
{
    stats.txPackets++;
    stats.txBytes += packet->GetSize();
}

// Reception callback
void
RxCallback(Ptr<const Packet> packet, const Address &addr)
{
    stats.rxPackets++;
    stats.rxBytes += packet->GetSize();
}

// Application for delay measurement
class DelayApp : public Application
{
public:
    DelayApp() : m_sendEvent(), m_running(false), m_interval(Seconds(1.0)) {}

    void Setup(Address address, uint16_t port, uint32_t count, Time interval)
    {
        m_peer = address;
        m_port = port;
        m_count = count;
        m_sent = 0;
        m_interval = interval;
    }

    void SetRxSocket(Ptr<Socket> sock)
    {
        m_rxSocket = sock;
        m_rxSocket->SetRecvCallback(MakeCallback(&DelayApp::HandleRead, this));
    }

private:
    virtual void StartApplication()
    {
        m_running = true;
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peer).GetIpv4(), m_port));
        SendPacket();
    }

    virtual void StopApplication()
    {
        m_running = false;
        if (m_socket)
        {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void SendPacket()
    {
        if (m_sent < m_count && m_running)
        {
            Ptr<Packet> p = Create<Packet>(100); // arbitrary payload size
            uint64_t now_us = Simulator::Now().GetMicroSeconds();
            p->AddByteTag(TimeTag(now_us));
            m_socket->Send(p);
            ++m_sent;
            Simulator::Schedule(m_interval, &DelayApp::SendPacket, this);
        }
        else if (m_socket)
        {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Address from;
        while (Ptr<Packet> packet = socket->RecvFrom(from))
        {
            stats.rxPackets++;
            stats.rxBytes += packet->GetSize();
            uint64_t sent_us = 0;
            TimeTag tag;
            if (packet->RemoveByteTag(tag))
            {
                sent_us = tag.m_time;
                double delay_ms = (Simulator::Now().GetMicroSeconds() - sent_us) / 1000.0;
                stats.totDelay += delay_ms;
            }
        }
    }

    struct TimeTag : public Tag
    {
        uint64_t m_time;
        TimeTag(uint64_t t = 0) : m_time(t) {}
        static TypeId GetTypeId() { static TypeId tid = TypeId("TimeTag").SetParent<Tag>(); return tid; }
        virtual uint32_t GetSerializedSize() const override { return sizeof(m_time); }
        virtual void Serialize(TagBuffer i) const override { i.WriteU64(m_time); }
        virtual void Deserialize(TagBuffer i) override { m_time = i.ReadU64(); }
        virtual void Print(std::ostream &os) const override { os << "time=" << m_time; }
    };

    Ptr<Socket> m_socket;
    Ptr<Socket> m_rxSocket;
    Address m_peer;
    uint16_t m_port;
    uint32_t m_count;
    uint32_t m_sent;
    bool m_running;
    EventId m_sendEvent;
    Time m_interval;
};

int main(int argc, char *argv[])
{
    double distance = 50.0;
    std::string dataFormat = "omnet";
    uint32_t runId = 1;
    uint32_t maxPackets = 50;
    double interval = 1.0;

    CommandLine cmd;
    cmd.AddValue("distance", "Distance between nodes (meters).", distance);
    cmd.AddValue("format", "Data output format: omnet or sqlite.", dataFormat);
    cmd.AddValue("run", "Run identifier.", runId);
    cmd.AddValue("maxPackets", "Packets to send.", maxPackets);
    cmd.AddValue("interval", "Packet interval (s).", interval);
    cmd.Parse(argc, argv);

    RngSeedManager::SetRun(runId);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    // UDP socket setup
    uint16_t port = 9999;
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
    InetSocketAddress localAddr = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(localAddr);
    recvSocket->SetRecvCallback(MakeCallback(&RxCallback));

    // App to send/receive and measure delay
    Ptr<DelayApp> app = CreateObject<DelayApp>();
    app->Setup(ifaces.GetAddress(1), port, maxPackets, Seconds(interval));
    nodes.Get(0)->AddApplication(app);

    app->SetRxSocket(recvSocket);

    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(1.0 + maxPackets * interval + 5));

    // Connect packet transmission for statistics
    devices.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback([](Ptr<const Packet> p) { TxCallback(p); }));

    Simulator::Stop(Seconds(1.0 + maxPackets * interval + 10.0));
    Simulator::Run();
    Simulator::Destroy();

    // Collect experiment data
    DataCollector collector;
    collector.DescribeRun("wifi-distance-experiment",
                          "Wi-Fi performance as a function of node distance",
                          "This experiment measures TX, RX, and delay statistics for varying node distances in Wi-Fi Ad-hoc mode.");
    collector.AddMetadata("author", "ns-3 user");
    collector.AddMetadata("distance", std::to_string(distance));
    collector.AddMetadata("run", std::to_string(runId));
    collector.AddMetadata("format", dataFormat);

    DataOutputCallback datapoints;
    datapoints.AddDataPoint("txPackets", stats.txPackets);
    datapoints.AddDataPoint("txBytes", stats.txBytes);
    datapoints.AddDataPoint("rxPackets", stats.rxPackets);
    datapoints.AddDataPoint("rxBytes", stats.rxBytes);
    datapoints.AddDataPoint("packetLoss", stats.txPackets - stats.rxPackets);
    datapoints.AddDataPoint("averageDelayMs", stats.rxPackets ? stats.totDelay / stats.rxPackets : 0.0);

    collector.AddDataOutputCallback(datapoints);

    if (dataFormat == "sqlite")
    {
        SqliteDataOutput sqlite;
        collector.AddDataOutput(&sqlite);
        collector.Collect();
    }
    else
    {
        OmnetDataOutput omnet;
        collector.AddDataOutput(&omnet);
        collector.Collect();
    }

    return 0;
}