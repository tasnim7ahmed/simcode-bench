#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-ospf-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfNetAnimExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Connect nodes with point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[10];
    // Manually creating a mesh-like topology
    devices[0] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    devices[1] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    devices[2] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    devices[3] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(3)));
    devices[4] = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(4)));
    devices[5] = p2p.Install(NodeContainer(nodes.Get(3), nodes.Get(4)));
    devices[6] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(3)));
    devices[7] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(4)));
    devices[8] = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    devices[9] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(4)));

    InternetStackHelper stack;
    stack.SetRoutingHelper(OspfRoutingHelper()); // Enable OSPF routing
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[10];

    for (uint32_t i = 0; i < 10; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up echo server on node 4
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up echo client on node 0, sending to node 4's IP
    UdpEchoClientHelper echoClient(interfaces[5].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Animation
    AsciiTraceHelper ascii;
    stack.EnableAsciiIpv4All(ascii.CreateFileStream("ospf-animation.tr"));
    AnimationInterface anim("ospf-animation.xml");
    anim.SetStartTime(Seconds(0));
    anim.SetStopTime(Seconds(10));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}