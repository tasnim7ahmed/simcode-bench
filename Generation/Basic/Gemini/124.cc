#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // 1. Create nodes
    ns3::NodeContainer allNodes;
    allNodes.Create(4); // Node 0: NodeA, Node 1: NodeB, Node 2: NodeC, Node 3: RouterNode

    ns3::Ptr<ns3::Node> nodeA = allNodes.Get(0);
    ns3::Ptr<ns3::Node> nodeB = allNodes.Get(1);
    ns3::Ptr<ns3::Node> nodeC = allNodes.Get(2);
    ns3::Ptr<ns3::Node> routerNode = allNodes.Get(3);

    // 2. Install CSMA devices (simulating a hub)
    ns3::CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csmaHelper.SetChannelAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(6560))); // Standard Ethernet delay

    ns3::NetDeviceContainer csmaDevices = csmaHelper.Install(allNodes);

    // 3. Install Internet Stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(allNodes);

    // 4. Assign IP Addresses to nodes and router
    ns3::Ipv4AddressHelper address;

    // Node A (Subnet 1: 10.1.1.0/24)
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ifA = address.Assign(csmaDevices.Get(0)); // NodeA gets 10.1.1.2

    // Node B (Subnet 2: 10.1.2.0/24)
    address.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ifB = address.Assign(csmaDevices.Get(1)); // NodeB gets 10.1.2.2

    // Node C (Subnet 3: 10.1.3.0/24)
    address.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ifC = address.Assign(csmaDevices.Get(2)); // NodeC gets 10.1.3.2

    // Assign multiple IP addresses to the router's single CSMA interface (acting as trunk)
    ns3::Ptr<ns3::Ipv4> ipv4Router = routerNode->GetObject<ns3::Ipv4>();
    int32_t routerCsmaIfId = ipv4Router->GetInterfaceForDevice(csmaDevices.Get(3));

    ipv4Router->AddAddress(routerCsmaIfId, ns3::Ipv4InterfaceAddress(ns3::Ipv4Address("10.1.1.1"), ns3::Ipv4Mask("255.255.255.0")));
    ipv4Router->AddAddress(routerCsmaIfId, ns3::Ipv4InterfaceAddress(ns3::Ipv4Address("10.1.2.1"), ns3::Ipv4Mask("255.255.255.0")));
    ipv4Router->AddAddress(routerCsmaIfId, ns3::Ipv4InterfaceAddress(ns3::Ipv4Address("10.1.3.1"), ns3::Ipv4Mask("255.255.255.0")));

    // Enable IP forwarding on the router so it can route traffic between subnets
    ipv4Router->SetForwarding(true);

    // 5. Set up routing tables
    // Populate global routing tables. This will make the router aware of its directly connected networks
    // (all three subnets via its single CSMA interface).
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Explicitly add default routes for end nodes to their respective gateways (router's IP on their subnet)
    // Node A's default route to 10.1.1.1
    ns3::Ptr<ns3::Ipv4> ipv4A = nodeA->GetObject<ns3::Ipv4>();
    ns3::Ptr<ns3::Ipv4RoutingProtocol> ipv4rpA = ipv4A->GetRoutingProtocol();
    ns3::Ptr<ns3::Ipv4GlobalRouting> globalRoutingA = ns3::DynamicCast<ns3::Ipv4GlobalRouting>(ipv4rpA);
    int32_t nodeACsmaIfId = ipv4A->GetInterfaceForDevice(csmaDevices.Get(0));
    globalRoutingA->GetStaticRouting()->SetDefaultRoute(ns3::Ipv4Address("10.1.1.1"), nodeACsmaIfId);

    // Node B's default route to 10.1.2.1
    ns3::Ptr<ns3::Ipv4> ipv4B = nodeB->GetObject<ns3::Ipv4>();
    ns3::Ptr<ns3::Ipv4RoutingProtocol> ipv4rpB = ipv4B->GetRoutingProtocol();
    ns3::Ptr<ns3::Ipv4GlobalRouting> globalRoutingB = ns3::DynamicCast<ns3::Ipv4GlobalRouting>(ipv4rpB);
    int32_t nodeBCsmaIfId = ipv4B->GetInterfaceForDevice(csmaDevices.Get(1));
    globalRoutingB->GetStaticRouting()->SetDefaultRoute(ns3::Ipv4Address("10.1.2.1"), nodeBCsmaIfId);

    // Node C's default route to 10.1.3.1
    ns3::Ptr<ns3::Ipv4> ipv4C = nodeC->GetObject<ns3::Ipv4>();
    ns3::Ptr<ns3::Ipv4RoutingProtocol> ipv4rpC = ipv4C->GetRoutingProtocol();
    ns3::Ptr<ns3::Ipv4GlobalRouting> globalRoutingC = ns3::DynamicCast<ns3::Ipv4GlobalRouting>(ipv4rpC);
    int32_t nodeCCsmaIfId = ipv4C->GetInterfaceForDevice(csmaDevices.Get(2));
    globalRoutingC->GetStaticRouting()->SetDefaultRoute(ns3::Ipv4Address("10.1.3.1"), nodeCCsmaIfId);

    // 6. Setup Applications (UdpEcho for traffic generation)
    // Servers on NodeA, NodeB, and NodeC
    ns3::UdpEchoServerHelper echoServerA(9);
    ns3::ApplicationContainer serverAppsA = echoServerA.Install(nodeA);
    serverAppsA.Start(ns3::Seconds(1.0));
    serverAppsA.Stop(ns3::Seconds(10.0));

    ns3::UdpEchoServerHelper echoServerB(9);
    ns3::ApplicationContainer serverAppsB = echoServerB.Install(nodeB);
    serverAppsB.Start(ns3::Seconds(1.0));
    serverAppsB.Stop(ns3::Seconds(10.0));

    ns3::UdpEchoServerHelper echoServerC(9);
    ns3::ApplicationContainer serverAppsC = echoServerC.Install(nodeC);
    serverAppsC.Start(ns3::Seconds(1.0));
    serverAppsC.Stop(ns3::Seconds(10.0));

    // Client on NodeA (10.1.1.2) sends to NodeB (10.1.2.2)
    ns3::UdpEchoClientHelper echoClientA(ifB.GetAddress(0), 9); // Target NodeB's IP
    echoClientA.SetAttribute("MaxPackets", ns3::UintegerValue(1));
    echoClientA.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClientA.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientAppsA = echoClientA.Install(nodeA);
    clientAppsA.Start(ns3::Seconds(2.0));
    clientAppsA.Stop(ns3::Seconds(10.0));

    // Client on NodeB (10.1.2.2) sends to NodeC (10.1.3.2)
    ns3::UdpEchoClientHelper echoClientB(ifC.GetAddress(0), 9); // Target NodeC's IP
    echoClientB.SetAttribute("MaxPackets", ns3::UintegerValue(1));
    echoClientB.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClientB.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientAppsB = echoClientB.Install(nodeB);
    clientAppsB.Start(ns3::Seconds(3.0));
    clientAppsB.Stop(ns3::Seconds(10.0));

    // Client on NodeC (10.1.3.2) sends to NodeA (10.1.1.2)
    ns3::UdpEchoClientHelper echoClientC(ifA.GetAddress(0), 9); // Target NodeA's IP
    echoClientC.SetAttribute("MaxPackets", ns3::UintegerValue(1));
    echoClientC.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClientC.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientAppsC = echoClientC.Install(nodeC);
    clientAppsC.Start(ns3::Seconds(4.0));
    clientAppsC.Stop(ns3::Seconds(10.0));

    // 7. Enable packet tracing to capture traffic
    csmaHelper.EnablePcap("hub-trunk-sim", csmaDevices); // Generates pcap files for all CSMA devices

    // 8. Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}