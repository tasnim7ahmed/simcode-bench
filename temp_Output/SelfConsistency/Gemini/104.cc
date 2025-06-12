#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetwork");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_INFO);
    LogComponent::SetLogLevel(
        "ThreeRouterNetwork",
        LogLevel::LOG_LEVEL_ALL);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(3);

    NodeContainer A = NodeContainer(nodes.Get(0));
    NodeContainer B = NodeContainer(nodes.Get(1));
    NodeContainer C = NodeContainer(nodes.Get(2));

    // Enable internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create channels A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2p.Install(A, B);
    NetDeviceContainer devicesBC = p2p.Install(B, C);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    // Static routing configuration
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Ptr<Ipv4StaticRouting> staticRoutingA = Ipv4RoutingHelper::GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    staticRoutingA->AddHostRouteTo(Ipv4Address("10.1.2.2"), Ipv4Address("10.1.1.2"), 1);

    Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    staticRoutingB->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.1.1"), 1);
    staticRoutingB->AddHostRouteTo(Ipv4Address("10.1.2.2"), Ipv4Address("10.1.2.2"), 1);

    Ptr<Ipv4StaticRouting> staticRoutingC = Ipv4RoutingHelper::GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    staticRoutingC->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.2.1"), 1);

    // Create OnOff application on node A
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(interfacesBC.GetAddress(1), 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer appA = onoff.Install(A);
    appA.Start(Seconds(1.0));
    appA.Stop(Seconds(10.0));

    // Create packet sink on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
    ApplicationContainer sinkApp = sink.Install(C);
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Tracing
    p2p.EnablePcapAll("three-router");

    // Animation Interface
    AnimationInterface anim("three-router.anim");
    anim.SetConstantPosition(nodes.Get(0), 1, 1);
    anim.SetConstantPosition(nodes.Get(1), 5, 1);
    anim.SetConstantPosition(nodes.Get(2), 9, 1);

    // Flowmonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	  std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
      }

    Simulator::Destroy();

    return 0;
}