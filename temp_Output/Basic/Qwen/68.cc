#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    std::string queueDiscType = "PfifoFast";
    uint64_t bottleneckBw = 10000000; // Default 10 Mbps
    Time bottleneckDelay = MilliSeconds(10);
    bool enableBql = false;
    std::string bqlMinThreshold = "1500";
    std::string bqlMaxThreshold = "1500";

    CommandLine cmd(__FILE__);
    cmd.AddValue("queueDiscType", "Queue disc type (PfifoFast, ARED, CoDel, FqCoDel, PIE)", queueDiscType);
    cmd.AddValue("bottleneckBw", "Bottleneck bandwidth in bps", bottleneckBw);
    cmd.AddValue("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits", enableBql);
    cmd.AddValue("bqlMinThreshold", "BQL minimum threshold (bytes)", bqlMinThreshold);
    cmd.AddValue("bqlMaxThreshold", "BQL maximum threshold (bytes)", bqlMaxThreshold);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    p2pAccess.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.1)));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bottleneckBw)));
    p2pBottleneck.SetChannelAttribute("Delay", TimeValue(bottleneckDelay));

    NetDeviceContainer devAccess1, devAccess2, devBottleneck;
    devAccess1 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));
    devBottleneck = p2pBottleneck.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAccess1 = address.Assign(devAccess1);

    address.NewNetwork();
    Ipv4InterfaceContainer ifBottleneck = address.Assign(devBottleneck);

    TrafficControlHelper tchBottleneck;
    if (queueDiscType == "PfifoFast") {
        tchBottleneck.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    } else if (queueDiscType == "ARED") {
        tchBottleneck.SetRootQueueDisc("ns3::AredQueueDisc");
    } else if (queueDiscType == "CoDel") {
        tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc");
    } else if (queueDiscType == "FqCoDel") {
        tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    } else if (queueDiscType == "PIE") {
        tchBottleneck.SetRootQueueDisc("ns3::PieQueueDisc");
    }

    if (enableBql) {
        tchBottleneck.SetQueueLimits("ns3::DynamicQueueLimits",
                                     "HoldTime", StringValue("0ms"),
                                     "MaxBytes", StringValue(bqlMaxThreshold),
                                     "MinBytes", StringValue(bqlMinThreshold));
    }

    tchBottleneck.Install(devBottleneck);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(ifBottleneck.GetAddress(1), port)));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("90Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1448));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(2));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Bidirectional flow
    Address sinkLocalAddress2(InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    PacketSinkHelper packetSinkHelper2("ns3::TcpSocketFactory", sinkLocalAddress2);
    ApplicationContainer sinkApp2 = packetSinkHelper2.Install(nodes.Get(2));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    OnOffHelper clientHelper2("ns3::TcpSocketFactory", Address());
    clientHelper2.SetAttribute("Remote", AddressValue(InetSocketAddress(ifAccess1.GetAddress(0), port + 1)));
    clientHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper2.SetAttribute("DataRate", DataRateValue(DataRate("90Mbps")));
    clientHelper2.SetAttribute("PacketSize", UintegerValue(1448));

    ApplicationContainer clientApp2 = clientHelper2.Install(nodes.Get(0));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(10.0));

    // Ping application
    V4PingHelper ping(nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ping.SetAttribute("Remote", Ipv4AddressValue(ifBottleneck.GetAddress(1)));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(0.0));
    pingApp.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("queue-disc-types.tr");

    p2pBottleneck.EnablePcapAll("bottleneck-link");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Goodput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}