#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routingA = staticRouting.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routingA->AddHostRouteTo(Ipv4Address("10.1.1.6"), interfacesAB.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> routingC = staticRouting.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routingC->AddHostRouteTo(Ipv4Address("10.1.1.2"), interfacesBC.GetAddress(1), 1);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    Packet::EnablePcapAll("three-router");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Flow Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("three-router.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}