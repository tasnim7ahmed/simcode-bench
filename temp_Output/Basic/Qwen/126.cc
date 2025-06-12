#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DvrSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create subnetwork hosts
    NodeContainer hostA = NodeContainer(nodes.Get(0));
    hostA.Create(1);
    NodeContainer hostB = NodeContainer(nodes.Get(2));
    hostB.Create(1);

    // Point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[3];

    // Router 0 <-> Router 1
    routerDevices[0] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    // Router 1 <-> Router 2
    routerDevices[1] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    // Router 0 <-> Router 2 (direct link for redundancy)
    routerDevices[2] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));

    // Host A to Router 0
    NetDeviceContainer hostADevices = p2p.Install(hostA, nodes.Get(0));
    // Host B to Router 2
    NetDeviceContainer hostBDevices = p2p.Install(hostB, nodes.Get(2));

    InternetStackHelper internet;
    DvRoutingHelper dvRouting;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(dvRouting, 0);

    internet.SetRoutingHelper(listRH);
    internet.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces[3];
    routerInterfaces[0] = ipv4.Assign(routerDevices[0]);
    ipv4.NewNetwork();
    routerInterfaces[1] = ipv4.Assign(routerDevices[1]);
    ipv4.NewNetwork();
    routerInterfaces[2] = ipv4.Assign(routerDevices[2]);
    ipv4.NewNetwork();

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer hostAInterfaces = ipv4.Assign(hostADevices);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer hostBInterfaces = ipv4.Assign(hostBDevices);

    // Enable routing protocol logging
    Simulator::Schedule(Seconds(0.1), &dvRouting.PrintRoutingTableAt, Seconds(5), nodes.Get(0), "router0.routes");
    Simulator::Schedule(Seconds(0.1), &dvRouting.PrintRoutingTableAt, Seconds(5), nodes.Get(1), "router1.routes");
    Simulator::Schedule(Seconds(0.1), &dvRouting.PrintRoutingTableAt, Seconds(5), nodes.Get(2), "router2.routes");

    // Install applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hostB.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(hostBInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(hostA.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable pcap tracing on all devices
    for (size_t i = 0; i < 3; ++i) {
        std::ostringstream oss;
        oss << "router" << i;
        routerDevices[i].Get(0)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PcapHelper::DropEvent, &PcapHelper::DefaultDropCallback));
        routerDevices[i].Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PcapHelper::DropEvent, &PcapHelper::DefaultDropCallback));
        routerDevices[i].Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&PcapHelper::TxEvent, &PcapHelper::DefaultTxCallback));
        routerDevices[i].Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&PcapHelper::TxEvent, &PcapHelper::DefaultTxCallback));
        PcapHelper::EnablePcap(oss.str(), routerDevices[i], true, true);
    }

    for (auto dev : hostADevices) {
        PcapHelper::EnablePcap("hostA", dev, true, true);
    }
    for (auto dev : hostBDevices) {
        PcapHelper::EnablePcap("hostB", dev, true, true);
    }

    AnimationInterface anim("dvr.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routes.xml", Seconds(0), Seconds(20), Seconds(0.25));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}