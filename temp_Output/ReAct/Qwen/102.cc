#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;
    uint16_t port = 9;
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 1024;
    double simulationTime = 10.0;

    NodeContainer nodes;
    nodes.Create(5);

    Names::Add("A", nodes.Get(0));
    Names::Add("B", nodes.Get(1));
    Names::Add("C", nodes.Get(2));
    Names::Add("D", nodes.Get(3));
    Names::Add("E", nodes.Get(4));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer linkAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer linkAC = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer linkBD = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer linkCE = p2p.Install(nodes.Get(2), nodes.Get(4));
    NetDeviceContainer linkDE = p2p.Install(nodes.Get(3), nodes.Get(4));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer ifAB = address.Assign(linkAB);
    address.NewNetwork();
    Ipv4InterfaceContainer ifAC = address.Assign(linkAC);
    address.NewNetwork();
    Ipv4InterfaceContainer ifBD = address.Assign(linkBD);
    address.NewNetwork();
    Ipv4InterfaceContainer ifCE = address.Assign(linkCE);
    address.NewNetwork();
    Ipv4InterfaceContainer ifDE = address.Assign(linkDE);

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ipv4StaticRouting *routingA = routingHelper.GetStaticRouting(ipv4A);
    routingA->SetMetric(1, 0);
    routingA->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 1);
    routingA->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 2);

    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ipv4StaticRouting *routingB = routingHelper.GetStaticRouting(ipv4B);
    routingB->SetMetric(1, 0);
    routingB->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 1);
    routingB->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 2);

    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ipv4StaticRouting *routingC = routingHelper.GetStaticRouting(ipv4C);
    routingC->SetMetric(1, 0);
    routingC->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 1);
    routingC->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 2);

    Ptr<Ipv4> ipv4D = nodes.Get(3)->GetObject<Ipv4>();
    Ipv4StaticRouting *routingD = routingHelper.GetStaticRouting(ipv4D);
    routingD->SetMetric(1, 0);
    routingD->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 1);

    Ptr<Ipv4> ipv4E = nodes.Get(4)->GetObject<Ipv4>();
    Ipv4StaticRouting *routingE = routingHelper.GetStaticRouting(ipv4E);
    routingE->SetMetric(1, 0);
    routingE->AddMulticastRoute(Ipv4Address("225.1.2.4"), Ipv4Address::GetAny(), 0, 1);

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;

    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        sinkHelper.SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));
        ApplicationContainer app = sinkHelper.Install(nodes.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simulationTime));
        sinkApps.Add(app);
    }

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        Address remoteAddress(InetSocketAddress(Ipv4Address("225.1.2.4"), port));
        onoff.SetAttribute("Remote", AddressValue(remoteAddress));
        onoff.SetAttribute("Local", AddressValue(Address(ifAB.GetAddress(0))));
        ApplicationContainer app = onoff.Install(nodes.Get(0));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));
        apps.Add(app);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (tracing) {
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("multicast-sim.tr"));
        p2p.EnablePcapAll("multicast-sim");
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalRxBytes = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalDropPackets = 0;

    for (uint32_t i = 0; i < sinkApps.GetN(); ++i) {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(i));
        uint64_t rxBytes = sink->GetTotalRxBytes();
        uint64_t rxPackets = sink->GetTotalRxPackets();
        uint64_t dropPackets = sink->GetTotalDiscardedPackets();

        totalRxBytes += rxBytes;
        totalRxPackets += rxPackets;
        totalDropPackets += dropPackets;

        NS_LOG_UNCOND("Receiver " << i << ": Received " << rxPackets << " packets (" << rxBytes << " bytes)");
        NS_LOG_UNCOND("Receiver " << i << ": Dropped " << dropPackets << " packets");
    }

    NS_LOG_UNCOND("Total Received Packets: " << totalRxPackets);
    NS_LOG_UNCOND("Total Received Bytes: " << totalRxBytes);
    NS_LOG_UNCOND("Total Dropped Packets: " << totalDropPackets);

    Simulator::Destroy();
    return 0;
}