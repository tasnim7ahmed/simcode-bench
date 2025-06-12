#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastNetworkSimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint16_t port = 9;
    double simulationTime = 10.0;
    Ipv4Address multicastGroup("225.1.2.4");

    if (verbose) {
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Assign node pointers
    Ptr<Node> A = nodes.Get(0);
    Ptr<Node> B = nodes.Get(1);
    Ptr<Node> C = nodes.Get(2);
    Ptr<Node> D = nodes.Get(3);
    Ptr<Node> E = nodes.Get(4);

    // Setup point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2p.Install(NodeContainer(A, B));
    NetDeviceContainer devicesAC = p2p.Install(NodeContainer(A, C));
    NetDeviceContainer devicesBD = p2p.Install(NodeContainer(B, D));
    NetDeviceContainer devicesCD = p2p.Install(NodeContainer(C, D));
    NetDeviceContainer devicesCE = p2p.Install(NodeContainer(C, E));
    NetDeviceContainer devicesDE = p2p.Install(NodeContainer(D, E));

    InternetStackHelper internet;
    Ipv4ListRoutingHelper listRH;

    // Static routing setup for each node
    Ipv4StaticRoutingHelper staticRh;
    listRH.Add(staticRh, 0);
    internet.SetRoutingHelper(listRH);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ipv4.Assign(devicesAB);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAC = ipv4.Assign(devicesAC);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBD = ipv4.Assign(devicesBD);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCD = ipv4.Assign(devicesCD);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCE = ipv4.Assign(devicesCE);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesDE = ipv4.Assign(devicesDE);

    // Multicast routing table entries
    Ptr<Ipv4StaticRouting> aStatic = staticRh.GetStaticRouting(A->GetObject<Ipv4>());
    aStatic->AddMulticastRoute(Ipv4Address::GetAny(), multicastGroup, interfacesAB.GetAddress(0), interfacesAB.Get(0)->GetIfIndex());
    aStatic->AddMulticastRoute(Ipv4Address::GetAny(), multicastGroup, interfacesAC.GetAddress(0), interfacesAC.Get(0)->GetIfIndex());

    // Set up sinks on B, C, D, E
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    ApplicationContainer sinkApps;

    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        sinkApps.Add(sinkHelper.Install(nodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Configure OnOff application on node A
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(multicastGroup, port)));
    onOffHelper.SetConstantRate(DataRate("1kbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer appContainer = onOffHelper.Install(A);
    appContainer.Start(Seconds(1.0));
    appContainer.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    p2p.EnablePcapAll("multicast_topology");

    // Run simulation
    Simulator::Run();

    // Output statistics
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