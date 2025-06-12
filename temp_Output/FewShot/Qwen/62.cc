#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

class TcpPacingTracer {
public:
    TcpPacingTracer(std::string cwndFile, std::string pacingRateFile, std::string txFile, std::string rxFile)
        : m_cWndStream(cwndFile.c_str(), std::ios::out),
          m_pacingRateStream(pacingRateFile.c_str(), std::ios::out),
          m_txStream(txFile.c_str(), std::ios::out),
          m_rxStream(rxFile.c_str(), std::ios::out) {}

    void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
        m_cWndStream.Write(Time::Now().GetSeconds(), sizeof(double));
        m_cWndStream.Write(newVal, sizeof(uint32_t));
        m_cWndStream.EndLine();
    }

    void PacingRateChange(Ptr<OutputStreamWrapper> stream, uint64_t oldRate, uint64_t newRate) {
        m_pacingRateStream.Write(Time::Now().GetSeconds(), sizeof(double));
        m_pacingRateStream.Write(newRate, sizeof(uint64_t));
        m_pacingRateStream.EndLine();
    }

    void TxTrace(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket) {
        m_txStream.Write(Time::Now().GetSeconds(), sizeof(double));
        m_txStream.Write(packet->GetSize(), sizeof(uint32_t));
        m_txStream.EndLine();
    }

    void RxTrace(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket) {
        m_rxStream.Write(Time::Now().GetSeconds(), sizeof(double));
        m_rxStream.Write(packet->GetSize(), sizeof(uint32_t));
        m_rxStream.EndLine();
    }

private:
    OutputStreamWrapper m_cWndStream;
    OutputStreamWrapper m_pacingRateStream;
    OutputStreamWrapper m_txStream;
    OutputStreamWrapper m_rxStream;
};

int main(int argc, char *argv[]) {
    double simulationTime = 10.0;
    bool enablePacing = true;
    uint32_t initialCwndSegments = 10;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
    cmd.AddValue("initialCwndSegments", "Initial congestion window size in segments", initialCwndSegments);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocketBase::MaxSegLifetime", DoubleValue(100));
    Config::SetDefault("ns3::TcpSocketBase::InitialCWnd", UintegerValue(initialCwndSegments));
    Config::SetDefault("ns3::TcpSocketBase::EnablePacing", BooleanValue(enablePacing));

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2pLowBandwidth;
    p2pLowBandwidth.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pLowBandwidth.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2pHighBandwidth;
    p2pHighBandwidth.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pHighBandwidth.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devN0N2 = p2pHighBandwidth.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devN1N2 = p2pHighBandwidth.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devN2N3 = p2pLowBandwidth.Install(nodes.Get(2), nodes.Get(3)); // Bottleneck link
    NetDeviceContainer devN3N4 = p2pHighBandwidth.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer devN3N5 = p2pHighBandwidth.Install(nodes.Get(3), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifN0N2 = address.Assign(devN0N2);
    address.NewNetwork();
    Ipv4InterfaceContainer ifN1N2 = address.Assign(devN1N2);
    address.NewNetwork();
    Ipv4InterfaceContainer ifN2N3 = address.Assign(devN2N3);
    address.NewNetwork();
    Ipv4InterfaceContainer ifN3N4 = address.Assign(devN3N4);
    address.NewNetwork();
    Ipv4InterfaceContainer ifN3N5 = address.Assign(devN3N5);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(4));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    InetSocketAddress remoteAddress(ifN3N4.GetAddress(0), sinkPort);
    remoteAddress.SetTos(0);

    OnOffHelper client("ns3::TcpSocketFactory", remoteAddress);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1448));
    client.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(simulationTime - 0.1));

    AsciiTraceHelper asciiTraceHelper;
    std::ofstream cwndStream("tcp-cwnd-trace.txt");
    std::ofstream pacingRateStream("tcp-pacing-rate-trace.txt");
    std::ofstream txStream("tcp-tx-trace.txt");
    std::ofstream rxStream("tcp-rx-trace.txt");

    TcpPacingTracer tracer("tcp-cwnd-trace.txt", "tcp-pacing-rate-trace.txt", "tcp-tx-trace.txt", "tcp-rx-trace.txt");

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                    MakeCallback(&TcpPacingTracer::RxTrace, &tracer));

    Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/CongestionWindow",
                    MakeCallback(&TcpPacingTracer::CwndChange, &tracer));

    Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/PacingRate",
                    MakeCallback(&TcpPacingTracer::PacingRateChange, &tracer));

    Config::Connect("/NodeList/*/TcpL4Protocol/SocketList/*/Tx",
                    MakeCallback(&TcpPacingTracer::TxTrace, &tracer));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
        std::cout << "  Mean Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? (iter->second.rxPackets - 1) : 1) << " s" << std::endl;
    }

    Simulator::Destroy();

    return 0;
}