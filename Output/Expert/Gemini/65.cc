#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enablePcap = false;
    bool enableDCTCP = false;
    std::string bottleneckQueueDiscType = "fqcodel";
    double simulationTime = 10.0;
    uint32_t maxBytes = 1000000;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableDCTCP", "Enable DCTCP", enableDCTCP);
    cmd.AddValue("bottleneckQueueDiscType", "Bottleneck queue disc type", bottleneckQueueDiscType);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("maxBytes", "Max Bytes for TCP BulkSend", maxBytes);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(enableDCTCP ? "ns3::TcpDctcp" : "ns3::TcpNewReno"));

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1d2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d1d3 = pointToPoint.Install(nodes.Get(1), nodes.Get(3));

    PointToPointHelper bottleneckPointToPoint;
    bottleneckPointToPoint.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    bottleneckPointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer bottleneckDevices = bottleneckPointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = address.Assign(d1d3);
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i2i3.GetAddress(0), port));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    TrafficControlHelper tch;
    QueueDiscContainer qdiscs;

    if (bottleneckQueueDiscType == "fqcodel") {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
        qdiscs = tch.Install(bottleneckDevices.Get(1));
    } else if (bottleneckQueueDiscType == "codel") {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
        qdiscs = tch.Install(bottleneckDevices.Get(1));
    }

    if (enablePcap) {
        pointToPoint.EnablePcapAll("scratch/traffic-control", false);
        bottleneckPointToPoint.EnablePcapAll("scratch/traffic-control", false);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}