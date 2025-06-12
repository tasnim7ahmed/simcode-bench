#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

class TcpMetricsTracer : public Object {
public:
    static TypeId GetTypeId(void);
    TcpMetricsTracer();
    virtual ~TcpMetricsTracer();

    void SetOutputStream(Ptr<OutputStreamWrapper> stream);
    void TraceCwnd(Ptr<Socket> localSocket, uint32_t oldVal, uint32_t newVal);
    void TracePacingRate(Ptr<Socket> localSocket, uint64_t oldVal, uint64_t newVal);
    void TraceRtt(Ptr<Socket> localSocket, Time oldVal, Time newVal);
    void TraceTx(Ptr<Packet const> packet, Ptr<TcpHeader const> header, Ptr<const Socket> socket);
    void TraceRx(Ptr<Packet const> packet, Ptr<const TcpHeader>, Ptr<const Socket> socket);

private:
    Ptr<OutputStreamWrapper> m_cwndStream;
    Ptr<OutputStreamWrapper> m_pacingStream;
    Ptr<OutputStreamWrapper> m_rttStream;
    Ptr<OutputStreamWrapper> m_txStream;
    Ptr<OutputStreamWrapper> m_rxStream;
};

TypeId TcpMetricsTracer::GetTypeId(void) {
    static TypeId tid = TypeId("TcpMetricsTracer")
                            .SetParent<Object>()
                            .AddConstructor<TcpMetricsTracer>();
    return tid;
}

TcpMetricsTracer::TcpMetricsTracer() {
    m_cwndStream = CreateFileStreamWrapper("cwnd.csv", std::ios::out);
    m_pacingStream = CreateFileStreamWrapper("pacing_rate.csv", std::ios::out);
    m_rttStream = CreateFileStreamWrapper("rtt.csv", std::ios::out);
    m_txStream = CreateFileStreamWrapper("tx_packets.csv", std::ios::out);
    m_rxStream = CreateFileStreamWrapper("rx_packets.csv", std::ios::out);
}

void TcpMetricsTracer::SetOutputStream(Ptr<OutputStreamWrapper> stream) {
    // Not used here
}

void TcpMetricsTracer::TraceCwnd(Ptr<Socket> localSocket, uint32_t oldVal, uint32_t newVal) {
    *m_cwndStream->GetStream() << Simulator::Now().GetSeconds() << "," << newVal << std::endl;
}

void TcpMetricsTracer::TracePacingRate(Ptr<Socket> localSocket, uint64_t oldVal, uint64_t newVal) {
    *m_pacingStream->GetStream() << Simulator::Now().GetSeconds() << "," << newVal << std::endl;
}

void TcpMetricsTracer::TraceRtt(Ptr<Socket> localSocket, Time oldVal, Time newVal) {
    *m_rttStream->GetStream() << Simulator::Now().GetSeconds() << "," << newVal.GetMicroSeconds() << std::endl;
}

void TcpMetricsTracer::TraceTx(Ptr<Packet const> packet, Ptr<TcpHeader const> header, Ptr<const Socket> socket) {
    *m_txStream->GetStream() << Simulator::Now().GetSeconds() << "," << packet->GetSize() << std::endl;
}

void TcpMetricsTracer::TraceRx(Ptr<Packet const> packet, Ptr<const TcpHeader>, Ptr<const Socket> socket) {
    *m_rxStream->GetStream() << Simulator::Now().GetSeconds() << "," << packet->GetSize() << std::endl;
}

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    bool enablePacing = true;
    uint32_t initialCwndPackets = 10;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
    cmd.AddValue("initialCwndPackets", "Initial congestion window size in packets", initialCwndPackets);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocketBase::EnablePacing", BooleanValue(enablePacing));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(initialCwndPackets));

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2p;

    NetDeviceContainer dev_n0n1;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
    dev_n0n1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    NetDeviceContainer dev_n1n2;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    dev_n1n2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    NetDeviceContainer dev_n2n3;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(20)));
    dev_n2n3 = p2p.Install(nodes.Get(2), nodes.Get(3));

    NetDeviceContainer dev_n3n4;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    dev_n3n4 = p2p.Install(nodes.Get(3), nodes.Get(4));

    NetDeviceContainer dev_n4n5;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
    dev_n4n5 = p2p.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n1 = address.Assign(dev_n0n1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n2n3 = address.Assign(dev_n2n3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n3n4 = address.Assign(dev_n3n4);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n4n5 = address.Assign(dev_n4n5);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(if_n5.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(5));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer sourceApp = onoff.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(simTime - 0.1));

    TcpMetricsTracer tracer;

    Config::Connect("/NodeList/0/ApplicationList/0/$ns3::OnOffApplication/Socket/Tx", MakeCallback(&TcpMetricsTracer::TraceTx, &tracer));
    Config::Connect("/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&TcpMetricsTracer::TraceRx, &tracer));
    Config::ConnectWithoutContext("/$ns3::TcpSocketBase/CongestionWindow", MakeCallback(&TcpMetricsTracer::TraceCwnd, &tracer));
    Config::ConnectWithoutContext("/$ns3::TcpSocketBase/PacingRate", MakeCallback(&TcpMetricsTracer::TracePacingRate, &tracer));
    Config::ConnectWithoutContext("/$ns3::TcpSocketBase/RTT", MakeCallback(&TcpMetricsTracer::TraceRtt, &tracer));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}