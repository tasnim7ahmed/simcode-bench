#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellExperiment");

// Function to set up the dumbbell topology
NodeContainer BuildDumbbell(uint32_t nLeft, uint32_t nRight, std::string bottleneckLinkDataRate, std::string bottleneckLinkDelay, std::string queueDisc) {
    // Create nodes
    NodeContainer leftNodes, rightNodes, routerLeft, routerRight;
    leftNodes.Create(nLeft);
    rightNodes.Create(1); // Single receiver
    routerLeft.Create(1);
    routerRight.Create(1);

    // Create point-to-point helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links
    NetDeviceContainer leftRouterDevices = pointToPoint.Install(leftNodes, routerLeft);
    NetDeviceContainer rightRouterDevices = pointToPoint.Install(rightNodes, routerRight);

    PointToPointHelper bottleneckPointToPoint;
    bottleneckPointToPoint.SetDeviceAttribute("DataRate", StringValue(bottleneckLinkDataRate));
    bottleneckPointToPoint.SetChannelAttribute("Delay", StringValue(bottleneckLinkDelay));

    NetDeviceContainer bottleneckDevices = bottleneckPointToPoint.Install(routerLeft, routerRight);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routerLeft);
    stack.Install(routerRight);

    // Assign addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftRouterInterfaces = address.Assign(leftRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer rightRouterInterfaces = address.Assign(rightRouterDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure QueueDisc
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDisc);

    QueueDiscContainer queueDiscs;
    queueDiscs.Add(tch.Install(bottleneckDevices).Get(0));
    queueDiscs.Add(tch.Install(bottleneckDevices).Get(1));

    // Return all nodes
    NodeContainer allNodes;
    allNodes.Add(leftNodes);
    allNodes.Add(rightNodes);
    allNodes.Add(routerLeft);
    allNodes.Add(routerRight);

    return allNodes;
}

void SetupTraffic(NodeContainer leftNodes, NodeContainer rightNodes, double startTime, double stopTime) {
    // Setup TCP traffic (BulkSendApplication)
    uint16_t port = 9; // Discard port
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(rightNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port));
    source.SetAttribute("SendSize", UintegerValue(1400));

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < leftNodes.GetN(); ++i) {
        sourceApps.Add(source.Install(leftNodes.Get(i)));
    }
    sourceApps.Start(Seconds(startTime));
    sourceApps.Stop(Seconds(stopTime));

    // Setup UDP traffic (OnOffApplication)
    uint16_t udpPort = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(rightNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), udpPort));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("10Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1400));

    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < leftNodes.GetN(); ++i) {
        sinkApps.Add(onoff.Install(leftNodes.Get(i)));
    }
    sinkApps.Start(Seconds(startTime));
    sinkApps.Stop(Seconds(stopTime));

    // Setup UDP sink on the receiver
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer appSink = sink.Install(rightNodes.Get(0));
    appSink.Start(Seconds(startTime));
    appSink.Stop(Seconds(stopTime));
}

void SetupCwndTracing(NodeContainer leftNodes, std::string fileName) {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(fileName + ".cwnd");

    for (uint32_t i = 0; i < leftNodes.GetN(); ++i) {
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(leftNodes.Get(i)->GetId()) + "/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow",
                                     MakeBoundCallback(&WriteCwndTrace, stream));
    }
}

void SetupQueueDiscTracing(NetDeviceContainer bottleneckDevices, std::string fileName) {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream(fileName + "_queue1.txt");
    Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream(fileName + "_queue2.txt");

    Config::ConnectWithoutContext("/ChannelList/0/DeviceList/" + std::to_string(bottleneckDevices.Get(0)->GetIfIndex()) + "/$ns3::PointToPointNetDevice/TxQueue/Queue/Size",
                                 MakeBoundCallback(&WriteQueueSizeTrace, stream1));

    Config::ConnectWithoutContext("/ChannelList/0/DeviceList/" + std::to_string(bottleneckDevices.Get(1)->GetIfIndex()) + "/$ns3::PointToPointNetDevice/TxQueue/Queue/Size",
                                 MakeBoundCallback(&WriteQueueSizeTrace, stream2));

}

static void WriteCwndTrace(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
    *stream << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

static void WriteQueueSizeTrace(Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue) {
    *stream << Simulator::Now().GetSeconds() << " " << newValue << std::endl;
}

void Experiment(std::string queueDisc) {
    // Simulation parameters
    uint32_t nLeft = 7;
    std::string bottleneckLinkDataRate = "10Mbps";
    std::string bottleneckLinkDelay = "20ms";
    double startTime = 1.0;
    double stopTime = 10.0;

    // Build the dumbbell topology
    NodeContainer allNodes = BuildDumbbell(nLeft, 1, bottleneckLinkDataRate, bottleneckLinkDelay, queueDisc);
    NodeContainer leftNodes;
    for(uint32_t i = 0; i < nLeft; ++i){
      leftNodes.Add(allNodes.Get(i));
    }
    NodeContainer rightNodes;
    rightNodes.Add(allNodes.Get(nLeft));

    NetDeviceContainer bottleneckDevices = allNodes.Get(nLeft + 2)->GetDevice(0);
    bottleneckDevices.Add(allNodes.Get(nLeft + 3)->GetDevice(0));

    // Setup traffic
    SetupTraffic(leftNodes, rightNodes, startTime, stopTime);

    // Setup Tracing
    std::string prefix = "dumbbell-" + queueDisc;
    SetupCwndTracing(leftNodes, prefix);
    SetupQueueDiscTracing(bottleneckDevices, prefix);

    // Run the simulation
    Simulator::Stop(Seconds(stopTime + 1)); // Add extra time for traces to flush
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    LogComponentEnable("DumbbellExperiment", LOG_LEVEL_INFO);

    // Enable checksum computation (recommended)
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Run the experiment with different queue disciplines
    Experiment("ns3::CobaltQueueDisc");
    Experiment("ns3::CoDelQueueDisc");

    return 0;
}