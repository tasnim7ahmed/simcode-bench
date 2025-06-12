#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RedQueueDiscExample");

int main(int argc, char *argv[]) {
    uint32_t maxPackets = 100;
    double simulationTime = 10.0;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("1000p")));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(500));
    Config::SetDefault("ns3::RedQueueDisc::Wait", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::Gentle", BooleanValue(false));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(0.002));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(5));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(15));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    NetDeviceContainer devices = pointToPoint.Install(nodes);
    QueueDiscContainer qdiscs = tch.Install(devices);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", StringValue("500kbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1000));
    onOffHelper.SetAttribute("MaxBytes", UintegerValue(10 * 1000 * 1000)); // 10 MB

    ApplicationContainer sourceApps;
    onOffHelper.SetAttribute("Remote", AddressValue(sinkAddress));
    sourceApps = onOffHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime - 0.1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("red-queue-disc.tr");
    pointToPoint.EnableAsciiAll(stream);

    pointToPoint.EnablePcapAll("red-queue-disc");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<NetDevice> device = devices.Get(i);
        Ptr<PointToPointNetDevice> p2pDevice = DynamicCast<PointToPointNetDevice>(device);
        if (p2pDevice) {
            uint32_t txBytes = p2pDevice->GetPhyTxDrop();
            std::cout << "Device " << i << " TX Drops: " << txBytes << std::endl;
        }
    }

    for (uint32_t i = 0; i < qdiscs.GetN(); ++i) {
        Ptr<QueueDisc> q = qdiscs.Get(i);
        std::cout << "QueueDisc " << i << " stats: " << q->GetStats() << std::endl;
        std::cout << "QueueDisc " << i << " current size: " << q->GetCurrentSize().GetValue() << std::endl;
    }

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowId id = iter->first;
        FlowMonitor::FlowStats s = iter->second;

        std::cout << "Flow ID: " << id << std::endl;
        std::cout << "  Tx Packets: " << s.txPackets << std::endl;
        std::cout << "  Rx Packets: " << s.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << s.lostPackets << std::endl;
        std::cout << "  Throughput: " << s.rxBytes * 8.0 / (s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps" << std::endl;
        std::cout << "  Mean Delay: " << s.delaySum.GetSeconds() / s.rxPackets << " s" << std::endl;
        std::cout << "  Jitter: " << (s.jitterSum.GetSeconds() / (s.rxPackets > 1 ? s.rxPackets - 1 : 1)) << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}