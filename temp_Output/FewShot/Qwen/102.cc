#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/multicast-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 5 nodes (A, B, C, D, E)
    NodeContainer nodes;
    nodes.Create(5);

    // Create point-to-point links between specific nodes: A-B, A-C, C-D, C-E
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1)); // A <-> B
    NetDeviceContainer devAC = p2p.Install(nodes.Get(0), nodes.Get(2)); // A <-> C
    NetDeviceContainer devCD = p2p.Install(nodes.Get(2), nodes.Get(3)); // C <-> D
    NetDeviceContainer devCE = p2p.Install(nodes.Get(2), nodes.Get(4)); // C <-> E

    // Install internet stack on all nodes
    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;

    // Disable global multicast routing for manual control
    stack.SetRoutingHelper(listRH); 
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = address.Assign(devAB);
    address.NewNetwork();
    Ipv4InterfaceContainer ifAC = address.Assign(devAC);
    address.NewNetwork();
    Ipv4InterfaceContainer ifCD = address.Assign(devCD);
    address.NewNetwork();
    Ipv4InterfaceContainer ifCE = address.Assign(devCE);

    // Configure static unicast routes
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    // Node A routing
    Ptr<Ipv4StaticRouting> aStaticRouting = ipv4RoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    aStaticRouting->AddMulticastRoute(Ipv4Address::GetAny(), Ipv4Address("224.0.0.1"), Ipv4Address::GetAny(), ifAC.GetInterfaceIndex(1));
    aStaticRouting->AddHostRouteTo(Ipv4Address("10.1.2.2"), Ipv4Address::GetBroadcast(), ifAB.GetInterfaceIndex(0)); // B
    aStaticRouting->AddHostRouteTo(Ipv4Address("10.1.3.2"), Ipv4Address::GetBroadcast(), ifAC.GetInterfaceIndex(0)); // C

    // Node B routing
    Ptr<Ipv4StaticRouting> bStaticRouting = ipv4RoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    bStaticRouting->AddNetworkRouteTo(Ipv4Address("224.0.0.0"), Ipv4Mask("240.0.0.0"), Ipv4Address::GetAny(), 1, 1);
    bStaticRouting->AddHostRouteTo(Ipv4Address("10.1.1.2"), Ipv4Address::GetBroadcast(), ifAB.GetInterfaceIndex(1)); // A
    bStaticRouting->AddHostRouteTo(Ipv4Address("10.1.3.2"), Ipv4Address::GetBroadcast(), ifAB.GetInterfaceIndex(1)); // via A -> C? (Not used)

    // Node C routing
    Ptr<Ipv4StaticRouting> cStaticRouting = ipv4RoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    cStaticRouting->AddMulticastRoute(Ipv4Address::GetAny(), Ipv4Address("224.0.0.1"), Ipv4Address::GetAny(), ifCD.GetInterfaceIndex(0));
    cStaticRouting->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address::GetBroadcast(), ifAC.GetInterfaceIndex(1)); // A
    cStaticRouting->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address::GetBroadcast(), ifAC.GetInterfaceIndex(1)); // B
    cStaticRouting->AddHostRouteTo(Ipv4Address("10.1.4.2"), Ipv4Address::GetBroadcast(), ifCD.GetInterfaceIndex(0)); // D
    cStaticRouting->AddHostRouteTo(Ipv4Address("10.1.5.2"), Ipv4Address::GetBroadcast(), ifCE.GetInterfaceIndex(0)); // E

    // Node D routing
    Ptr<Ipv4StaticRouting> dStaticRouting = ipv4RoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    dStaticRouting->AddNetworkRouteTo(Ipv4Address("224.0.0.0"), Ipv4Mask("240.0.0.0"), Ipv4Address::GetAny(), 1, 1);
    dStaticRouting->AddHostRouteTo(Ipv4Address("10.1.3.1"), Ipv4Address::GetBroadcast(), ifCD.GetInterfaceIndex(1)); // C

    // Node E routing
    Ptr<Ipv4StaticRouting> eStaticRouting = ipv4RoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    eStaticRouting->AddNetworkRouteTo(Ipv4Address("224.0.0.0"), Ipv4Mask("240.0.0.0"), Ipv4Address::GetAny(), 1, 1);
    eStaticRouting->AddHostRouteTo(Ipv4Address("10.1.3.1"), Ipv4Address::GetBroadcast(), ifCE.GetInterfaceIndex(1)); // C

    // Set up multicast group address
    Ipv4Address multicastGroup("224.0.0.1");

    // Install packet sinks on nodes B, C, D, and E to receive packets from node A
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    ApplicationContainer sinkApps;

    sinkApps.Add(sinkHelper.Install(nodes.Get(1))); // Node B
    sinkApps.Add(sinkHelper.Install(nodes.Get(2))); // Node C
    sinkApps.Add(sinkHelper.Install(nodes.Get(3))); // Node D
    sinkApps.Add(sinkHelper.Install(nodes.Get(4))); // Node E

    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Set up OnOff application on node A to send UDP packets to the multicast group
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0)); // Node A
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("multicast-sim.tr"));
    p2p.EnablePcapAll("multicast-sim");

    // Run simulation
    Simulator::Run();

    // Output packet statistics
    Ptr<PacketSink> sinkB = DynamicCast<PacketSink>(sinkApps.Get(0));
    NS_LOG_INFO("Node B received " << sinkB->GetTotalRx() << " bytes");

    Ptr<PacketSink> sinkC = DynamicCast<PacketSink>(sinkApps.Get(1));
    NS_LOG_INFO("Node C received " << sinkC->GetTotalRx() << " bytes");

    Ptr<PacketSink> sinkD = DynamicCast<PacketSink>(sinkApps.Get(2));
    NS_LOG_INFO("Node D received " << sinkD->GetTotalRx() << " bytes");

    Ptr<PacketSink> sinkE = DynamicCast<PacketSink>(sinkApps.Get(3));
    NS_LOG_INFO("Node E received " << sinkE->GetTotalRx() << " bytes");

    Simulator::Destroy();

    return 0;
}