#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/animation-interface.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfRoutingSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for OSPF (optional)
    LogComponentEnable("OspfRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv4L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes in a simple mesh-like topology
    NetDeviceContainer devices[6];

    devices[0] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    devices[1] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    devices[2] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(3)));
    devices[3] = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    devices[4] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(4)));
    devices[5] = p2p.Install(NodeContainer(nodes.Get(3), nodes.Get(4)));

    // Install Internet stack with OSPF routing
    InternetStackHelper stack;
    OspfRoutingHelper routing;
    stack.SetRoutingHelper(routing);  // Use OSPF routing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[6];

    address.SetBase("10.0.0.0", "255.255.255.0");
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

    // Set up a UDP echo server and client to generate traffic
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[5].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable animation
    AnimationInterface anim("ospf-routing-animation.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 5.0, 5.0);
    anim.SetConstantPosition(nodes.Get(2), -5.0, 5.0);
    anim.SetConstantPosition(nodes.Get(3), 0.0, 10.0);
    anim.SetConstantPosition(nodes.Get(4), 0.0, 15.0);

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("ospf-routing");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}