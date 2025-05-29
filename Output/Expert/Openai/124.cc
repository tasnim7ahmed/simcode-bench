#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Nodes: 0 - Hub/Router, 1 - Subnet1, 2 - Subnet2, 3 - Subnet3
    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer subnet1Node, subnet2Node, subnet3Node;
    subnet1Node.Create(1);
    subnet2Node.Create(1);
    subnet3Node.Create(1);

    // Subnet1: hubNode <-> subnet1Node
    NodeContainer subnet1;
    subnet1.Add(hubNode.Get(0));
    subnet1.Add(subnet1Node.Get(0));

    // Subnet2: hubNode <-> subnet2Node
    NodeContainer subnet2;
    subnet2.Add(hubNode.Get(0));
    subnet2.Add(subnet2Node.Get(0));

    // Subnet3: hubNode <-> subnet3Node
    NodeContainer subnet3;
    subnet3.Add(hubNode.Get(0));
    subnet3.Add(subnet3Node.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install on each subnet
    NetDeviceContainer devices1 = csma.Install(subnet1);
    NetDeviceContainer devices2 = csma.Install(subnet2);
    NetDeviceContainer devices3 = csma.Install(subnet3);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(hubNode);
    internet.Install(subnet1Node);
    internet.Install(subnet2Node);
    internet.Install(subnet3Node);

    // Assign IP addresses
    Ipv4AddressHelper ipv4_1, ipv4_2, ipv4_3;
    ipv4_1.SetBase("10.1.1.0", "255.255.255.0");
    ipv4_2.SetBase("10.1.2.0", "255.255.255.0");
    ipv4_3.SetBase("10.1.3.0", "255.255.255.0");

    Ipv4InterfaceContainer iface1 = ipv4_1.Assign(devices1);
    Ipv4InterfaceContainer iface2 = ipv4_2.Assign(devices2);
    Ipv4InterfaceContainer iface3 = ipv4_3.Assign(devices3);

    // Set up routing (hub node acts as router)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set default route for each node to router IP in its subnet
    Ptr<Ipv4StaticRouting> staticRouting1 = Ipv4RoutingHelper().GetStaticRouting(subnet1Node.Get(0)->GetObject<Ipv4>());
    staticRouting1->SetDefaultRoute(iface1.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> staticRouting2 = Ipv4RoutingHelper().GetStaticRouting(subnet2Node.Get(0)->GetObject<Ipv4>());
    staticRouting2->SetDefaultRoute(iface2.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> staticRouting3 = Ipv4RoutingHelper().GetStaticRouting(subnet3Node.Get(0)->GetObject<Ipv4>());
    staticRouting3->SetDefaultRoute(iface3.GetAddress(0), 1);

    // Set up application: ICMP echo from subnet1Node to subnet2Node and subnet3Node
    uint16_t port = 9;
    // Echo server on subnet2Node
    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApps2 = echoServer2.Install(subnet2Node.Get(0));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));
    // Echo server on subnet3Node
    UdpEchoServerHelper echoServer3(port);
    ApplicationContainer serverApps3 = echoServer3.Install(subnet3Node.Get(0));
    serverApps3.Start(Seconds(1.0));
    serverApps3.Stop(Seconds(10.0));

    // Echo client to subnet2Node
    UdpEchoClientHelper echoClient2(iface2.GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps2 = echoClient2.Install(subnet1Node.Get(0));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(5.0));

    // Echo client to subnet3Node
    UdpEchoClientHelper echoClient3(iface3.GetAddress(1), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps3 = echoClient3.Install(subnet1Node.Get(0));
    clientApps3.Start(Seconds(6.0));
    clientApps3.Stop(Seconds(9.0));

    // Enable packet capture
    csma.EnablePcapAll("hub-vlan");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}