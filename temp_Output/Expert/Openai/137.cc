#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-Point Link 1 (n0 <-> n1)
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer dev0 = p2p.Install(n0n1);

    // Point-to-Point Link 2 (n1 <-> n2)
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev1 = p2p.Install(n1n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    // IP assignment: subnet 10.1.1.0/24 for n0-n1, 10.1.2.0/24 for n1-n2
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface0 = address.Assign(dev0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface1 = address.Assign(dev1);

    // Enable routing on router node (n1)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on node 2
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on node 0, destination IP: iface1.GetAddress(1)
    UdpEchoClientHelper echoClient(iface1.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}