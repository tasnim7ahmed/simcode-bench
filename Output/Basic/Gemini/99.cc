#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetAttribute("UdpClient", "MaxPackets", UintegerValue(1000));

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routerBStaticRouting = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4> ());
    routerBStaticRouting->AddHostRouteTo(Ipv4Address("10.1.1.1"), 1, interfacesAB.GetInterface(0)->GetIp());
    routerBStaticRouting->AddHostRouteTo(Ipv4Address("10.1.2.2"), 2, interfacesBC.GetInterface(1)->GetIp());

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfacesBC.GetAddress(1), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    p2p.EnablePcapAll("three-router");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("three-router.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}