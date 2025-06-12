#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Connect nodes with point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[6];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(0), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(1), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[4] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[5] = p2p.Install(nodes.Get(0), nodes.Get(3));

    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;
    Ipv4OspfRoutingHelper ospfRH;

    listRH.Add(ospfRH, 0); // Add OSPF as a routing protocol
    stack.SetRoutingHelper(listRH);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[6];

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);
    address.NewNetwork();
    interfaces[4] = address.Assign(devices[4]);
    address.NewNetwork();
    interfaces[5] = address.Assign(devices[5]);

    // Create UDP ping between node 0 and node 3
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[5].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable animation
    AnimationInterface anim("ospf-routing.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 30.0);
    anim.SetConstantPosition(nodes.Get(2), 30.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 40.0, 30.0);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}