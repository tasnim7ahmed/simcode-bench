#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

class TcpMetricsTracer {
public:
    TcpMetricsTracer(std::string cwndFile, std::string pacingRateFile, std::string txFile, std::string rxFile)
        : m_cwndStream(cwndFile.c_str(), std::ios::out),
          m_pacingRateStream(pacingRateFile.c_str(), std::ios::out),
          m_txStream(txFile.c_str(), std::ios::out),
          m_rxStream(rxFile.c_str(), std::ios::out) {}

    void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
        m_cwndStream.Write(Time::Now().GetSeconds(), newVal);
    }

    void PacingRateChange(uint64_t bytesPerSecond, double now) {
        m_pacingRateStream.Write(now, bytesPerSecond);
    }

    void TxPacket(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const Socket> socket) {
        m_txStream.Write(Time::Now().GetSeconds(), packet->GetSize());
    }

    void RxPacket(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const Socket> socket) {
        m_rxStream.Write(Time::Now().GetSeconds(), packet->GetSize());
    }

private:
    OutputStreamWrapper m_cwndStream;
    OutputStreamWrapper m_pacingRateStream;
    OutputStreamWrapper m_txStream;
    OutputStreamWrapper m_rxStream;
};

int main(int argc, char* argv[]) {
    bool enablePacing = true;
    double simTime = 10.0;
    uint32_t initialCWnd = 10;
    std::string transportProt = "TcpNewReno";

    CommandLine cmd(__FILE__);
    cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("initialCWnd", "Initial congestion window size (segments)", initialCWnd);
    cmd.AddValue("transportProt", "Transport protocol to use: TcpNewReno, TcpBbr, etc.", transportProt);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocketBase::InitialCWnd", UintegerValue(initialCWnd));
    Config::SetDefault("ns3::TcpSocket::Pacing", BooleanValue(enablePacing));

    if (transportProt == "TcpBbr") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
    } else if (transportProt == "TcpNewReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    }

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2p;

    // n0 <-> n1
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // n1 <-> n2
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // n2 <-> n3 (bottleneck)
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(20)));
    NetDeviceContainer dev23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // n3 <-> n4
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
    NetDeviceContainer dev34 = p2p.Install(nodes.Get(3), nodes.Get(4));

    // n4 <-> n5
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer dev45 = p2p.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(dev01);

    address.NewNetwork();
    Ipv4InterfaceContainer if12 = address.Assign(dev12);

    address.NewNetwork();
    Ipv4InterfaceContainer if23 = address.Assign(dev23);

    address.NewNetwork();
    Ipv4InterfaceContainer if34 = address.Assign(dev34);

    address.NewNetwork();
    Ipv4InterfaceContainer if45 = address.Assign(dev45);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(if45.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(5));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
    source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.5));
    sourceApp.Stop(Seconds(simTime - 0.1));

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-pacing.tr"));
    p2p.EnablePcapAll("tcp-pacing");

    TcpMetricsTracer tracer("cwnd.dat", "pacing-rate.dat", "tx-packets.dat", "rx-packets.dat");

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Tx", MakeCallback(&TcpMetricsTracer::TxPacket, &tracer));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&TcpMetricsTracer::RxPacket, &tracer));

    Config::ConnectWithoutContext("/NodeList/*/TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&TcpMetricsTracer::CwndChange, &tracer));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->Start(Seconds(0.5));
    monitor->Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Info: " << iter->second);
    }

    Simulator::Destroy();
    return 0;
}