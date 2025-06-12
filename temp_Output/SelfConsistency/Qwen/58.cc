#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBbrNetworkSimulation");

class TcpBbrTracer {
public:
    static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
        *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal / 1448.0 << std::endl;
    }

    static void QueueSizeChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
        *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
    }

    static void ThroughputMonitor(Ptr<FlowMonitor> monitor, Ptr<OutputStreamWrapper> stream) {
        double cumThroughput = 0;
        monitor->CheckForLostPackets();
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (auto it = stats.begin(); it != stats.end(); ++it) {
            cumThroughput += it->second.rxBytes * 8.0 / 1e6; // Mbps
        }
        *stream->GetStream() << Simulator::Now().GetSeconds() << " " << cumThroughput << std::endl;
        Simulator::Schedule(Seconds(0.5), &ThroughputMonitor, monitor, stream);
    }
};

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    Config::SetDefault("ns3::TcpBbr::RttProbeInterval", TimeValue(Seconds(10)));

    NodeContainer nodes;
    nodes.Create(4); // sender, R1, R2, receiver

    NodeContainer link1 = NodeContainer(nodes.Get(0), nodes.Get(1)); // sender to R1
    NodeContainer link2 = NodeContainer(nodes.Get(1), nodes.Get(2)); // R1 to R2 (bottleneck)
    NodeContainer link3 = NodeContainer(nodes.Get(2), nodes.Get(3)); // R2 to receiver

    PointToPointHelper p2pHighBandwidth;
    p2pHighBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
    p2pHighBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
    p2pHighBandwidth.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    PointToPointHelper p2pLowBandwidth;
    p2pLowBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2pLowBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    p2pLowBandwidth.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    NetDeviceContainer devices1 = p2pHighBandwidth.Install(link1);
    NetDeviceContainer devices2 = p2pLowBandwidth.Install(link2);
    NetDeviceContainer devices3 = p2pHighBandwidth.Install(link3);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces3.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(100.0));

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4194304));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("100Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1448));
    ApplicationContainer senderApp = onoff.Install(nodes.Get(0));
    senderApp.Start(Seconds(0.5));
    senderApp.Stop(Seconds(100.0));

    AsciiTraceHelper asciiTraceHelper;
    p2pHighBandwidth.EnablePcapAll("tcp-bbr-sim-highlink");
    p2pLowBandwidth.EnablePcapAll("tcp-bbr-sim-bottleneck");

    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("tcp-bbr-cwnd-trace.txt");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&TcpBbrTracer::CwndChange, cwndStream));

    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("tcp-bbr-queue-size-trace.txt");
    devices2.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&TcpBbrTracer::QueueSizeChange, queueStream));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<OutputStreamWrapper> throughputStream = asciiTraceHelper.CreateFileStream("tcp-bbr-throughput-trace.txt");
    Simulator::Schedule(Seconds(1.0), &TcpBbrTracer::ThroughputMonitor, monitor, throughputStream);

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}