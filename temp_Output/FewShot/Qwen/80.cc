#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TbfExample");

double bucketSize1 = 1500;
double bucketSize2 = 1500;
double rate = 512000; // bits per second
double peakRate = 1024000; // bits per second
double simulationTime = 10;

void TokenTrace(uint32_t oldVal, uint32_t newVal)
{
    NS_LOG_INFO("Tokens changed from " << oldVal << " to " << newVal);
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("bucketSize1", "First Bucket Size in Bytes", bucketSize1);
    cmd.AddValue("bucketSize2", "Second Bucket Size in Bytes", bucketSize2);
    cmd.AddValue("rate", "Token Rate in bps", rate);
    cmd.AddValue("peakRate", "Peak Rate in bps", peakRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    TrafficControlHelper tch;
    uint16_t handle = 1;
    uint16_t devtxqueuelen = 1000;
    tch.SetRootQueueDisc("ns3::TbfQueueDisc",
                         "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, devtxqueuelen)),
                         "Burst", UintegerValue((uint32_t)bucketSize1),
                         "Mtu", UintegerValue((uint32_t)bucketSize2),
                         "Rate", DataRateValue(DataRate(rate)),
                         "PeakRate", DataRateValue(DataRate(peakRate)));

    NetDeviceContainer devices = pointToPoint.Install(nodes);
    tch.Install(devices);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff.SetConstantRate(DataRate(rate));
    onoff.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
    apps = sink.Install(nodes.Get(1));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Config::Connect("/NodeList/0/DeviceList/0/TxQueue/TokenDrop", MakeCallback(&TokenTrace));
    Config::Connect("/NodeList/0/DeviceList/0/TxQueue/TokenPktDrop", MakeCallback(&TokenTrace));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}