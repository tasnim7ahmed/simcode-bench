#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Interconnect nodes in a simple topology (mesh-like)
    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d34 = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer d03 = p2p.Install(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer d14 = p2p.Install(nodes.Get(1), nodes.Get(4));

    // Install Internet stack with OSPF
    OspfRoutingHelper ospfRouting;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospfRouting);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> iface;

    address.SetBase("10.0.1.0", "255.255.255.0");
    iface.push_back(address.Assign(d01));
    address.SetBase("10.0.2.0", "255.255.255.0");
    iface.push_back(address.Assign(d12));
    address.SetBase("10.0.3.0", "255.255.255.0");
    iface.push_back(address.Assign(d23));
    address.SetBase("10.0.4.0", "255.255.255.0");
    iface.push_back(address.Assign(d34));
    address.SetBase("10.0.5.0", "255.255.255.0");
    iface.push_back(address.Assign(d03));
    address.SetBase("10.0.6.0", "255.255.255.0");
    iface.push_back(address.Assign(d14));

    // Set up visualization positions
    AnimationInterface anim("ospf-netanim.xml");
    anim.SetConstantPosition(nodes.Get(0), 50, 50);
    anim.SetConstantPosition(nodes.Get(1), 150, 100);
    anim.SetConstantPosition(nodes.Get(2), 250, 150);
    anim.SetConstantPosition(nodes.Get(3), 150, 200);
    anim.SetConstantPosition(nodes.Get(4), 300, 100);

    // UdpEchoServer on Node 4
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UdpEchoClient from Node 0 to Node 4
    Ipv4Address serverAddr = nodes.Get(4)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
    UdpEchoClientHelper echoClient(serverAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable OSPF log components if desired
    // LogComponentEnable("OspfRoutingProtocol", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}