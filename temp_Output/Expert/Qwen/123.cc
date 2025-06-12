#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSquareTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    OspfHelper ospfRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospfRouting, 0);
    stack.SetRoutingHelper(listRouting);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces30 = address.Assign(devices30);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces12.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    p2p.EnablePcapAll("ospf-square");

    Simulator::Schedule(Seconds(9.5), &Ipv4RoutingHelper::PrintRoutingTableAllAt, Seconds(9.5), nodes);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}