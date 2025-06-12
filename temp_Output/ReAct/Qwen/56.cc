#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpBottleneckSimulation");

int main(int argc, char *argv[]) {
    uint32_t sendersT1 = 30;
    uint32_t sendersT2 = 20;
    uint32_t totalSenders = sendersT1 + sendersT2;
    double simulationTime = 5.0;
    double convergenceTime = 3.0;
    double measurementDuration = 1.0;
    double startTimeWindow = 0.0;
    double endTimeWindow = 1.0;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketBase::GetTypeId()));
    Config::SetDefault("ns3::RedQueueDisc::Mode", EnumValue(RedQueueDisc::QUEUE_DISC_MODE_PACKETS));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(5));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(15));
    Config::SetDefault("ns3::RedQueueDisc::QueueLimit", UintegerValue(25));
    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));

    NodeContainer hosts;
    hosts.Create(totalSenders);

    NodeContainer switches;
    switches.Create(2);

    NodeContainer router;
    router.Create(1);

    PointToPointHelper p2pHostSwitch;
    p2pHostSwitch.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    p2pHostSwitch.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    PointToPointHelper bottleneckT1T2;
    bottleneckT1T2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    bottleneckT1T2.SetChannelAttribute("Delay", TimeValue(MicroSeconds(10)));
    bottleneckT1T2.SetQueue("ns3::RedQueueDisc");

    PointToPointHelper bottleneckT2R1;
    bottleneckT2R1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    bottleneckT2R1.SetChannelAttribute("Delay", TimeValue(MicroSeconds(10)));
    bottleneckT2R1.SetQueue("ns3::RedQueueDisc");

    NetDeviceContainer hostDevices[totalSenders];
    for (uint32_t i = 0; i < totalSenders; ++i) {
        hostDevices[i] = p2pHostSwitch.Install(hosts.Get(i), switches.Get(i < sendersT1 ? 0 : 1));
    }

    NetDeviceContainer switchT1T2 = bottleneckT1T2.Install(switches.Get(0), switches.Get(1));
    NetDeviceContainer switchT2Router = bottleneckT2R1.Install(switches.Get(1), router.Get(0));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer hostInterfaces[totalSenders];
    for (uint32_t i = 0; i < totalSenders; ++i) {
        Ipv4InterfaceContainer ifc = address.Assign(hostDevices[i]);
        hostInterfaces[i] = ifc;
        address.NewNetwork();
    }

    Ipv4InterfaceContainer switchT1T2Ifc = address.Assign(switchT1T2);
    address.NewNetwork();
    Ipv4InterfaceContainer switchT2RouterIfc = address.Assign(switchT2Router);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    ApplicationContainer sinkApps;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sinkApps = sinkHelper.Install(router.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    ApplicationContainer sourceApps;
    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1448));

    for (uint32_t i = 0; i < totalSenders; ++i) {
        double startTime = startTimeWindow + (endTimeWindow - startTimeWindow) * ((double)i / totalSenders);
        AddressValue remoteAddress(InetSocketAddress(switchT2RouterIfc.GetAddress(1), port));
        onoff.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = onoff.Install(hosts.Get(i));
        app.Start(Seconds(startTime));
        app.Stop(Seconds(simulationTime));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout.precision(4);
    std::cout << "Throughput per flow (Mbps):" << std::endl;
    double totalThroughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->first == 0) continue;
        double rxBytes = i->second.rxBytes * 8.0 / measurementDuration / 1e6;
        std::cout << "Flow " << i->first << ": " << rxBytes << std::endl;
        totalThroughput += rxBytes;
    }

    std::cout << "Aggregate throughput (Mbps): " << totalThroughput << std::endl;

    double fairnessNumerator = 0.0;
    double fairnessDenominator = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->first == 0) continue;
        double rate = i->second.rxBytes * 8.0 / measurementDuration / 1e6;
        fairnessNumerator += rate;
        fairnessDenominator += rate * rate;
    }
    double fairness = (fairnessNumerator * fairnessNumerator) / (stats.size() * fairnessDenominator);
    std::cout << "Fairness Index: " << fairness << std::endl;

    Ptr<Ipv4QueueDiscItem> item;
    QueueDiscContainer qdiscs = switches.Get(0)->GetDevice(1)->GetObject<PointToPointNetDevice>()->GetQueueDisc();
    if (qdiscs.GetNQueueDiscs() > 0) {
        Ptr<QueueDisc> qd = qdiscs.Get(0);
        std::cout << "Queue size at T1-T2 bottleneck: " << qd->GetNPackets() << " packets" << std::endl;
        std::cout << "Marked packets at T1-T2: " << qd->GetStats().GetNDroppedPackets(QueueDiscClass::DROP_ECN_MARKED) << std::endl;
    }

    qdiscs = switches.Get(1)->GetDevice(2)->GetObject<PointToPointNetDevice>()->GetQueueDisc();
    if (qdiscs.GetNQueueDiscs() > 0) {
        Ptr<QueueDisc> qd = qdiscs.Get(0);
        std::cout << "Queue size at T2-R1 bottleneck: " << qd->GetNPackets() << " packets" << std::endl;
        std::cout << "Marked packets at T2-R1: " << qd->GetStats().GetNDroppedPackets(QueueDiscClass::DROP_ECN_MARKED) << std::endl;
    }

    Simulator::Destroy();
    return 0;
}