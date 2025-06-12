#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarNetworkSimulation");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetMinimumLogLevel(LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6);

    NodeContainer serverNode = NodeContainer(nodes.Get(0));
    NodeContainer clientNodes;
    for (uint32_t i = 1; i < 6; ++i) {
        clientNodes.Add(nodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 1; i < 6; ++i) {
        NodeContainer link = NodeContainer(nodes.Get(0), nodes.Get(i));
        devices.Add(p2p.Install(link));
    }

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    source.SetAttribute("SendSize", UintegerValue(1400));
    source.SetAttribute("MaxBytes", UintegerValue(1000000));

    ApplicationContainer sourceApps;
    for (uint32_t i = 1; i < 6; ++i) {
        sourceApps.Add(source.Install(nodes.Get(i)));
    }
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("star-network.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 20, 20);
    anim.SetConstantPosition(nodes.Get(2), 30, 20);
    anim.SetConstantPosition(nodes.Get(3), 20, 30);
    anim.SetConstantPosition(nodes.Get(4), 30, 30);
    anim.SetConstantPosition(nodes.Get(5), 40, 30);

    Simulator::Run();

    monitor->CheckForLostPackets();
    std::ofstream os;
    os.open("star-network.flowmon");
    monitor->SerializeToStream(os, true);
    os.close();

    Simulator::Destroy();
    return 0;
}