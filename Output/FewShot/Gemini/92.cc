#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer srcNode, destNode, rtr1, rtr2, destRtr;
    srcNode.Create(1);
    destNode.Create(1);
    rtr1.Create(1);
    rtr2.Create(1);
    destRtr.Create(1);

    // Create point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes
    NetDeviceContainer srcRtr1Devs = p2p.Install(srcNode.Get(0), rtr1.Get(0));
    NetDeviceContainer srcRtr2Devs = p2p.Install(srcNode.Get(0), rtr2.Get(0));
    NetDeviceContainer rtr1DestRtrDevs = p2p.Install(rtr1.Get(0), destRtr.Get(0));
    NetDeviceContainer rtr2DestRtrDevs = p2p.Install(rtr2.Get(0), destRtr.Get(0));
    NetDeviceContainer destRtrDestDevs = p2p.Install(destRtr.Get(0), destNode.Get(0));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(srcNode);
    stack.Install(destNode);
    stack.Install(rtr1);
    stack.Install(rtr2);
    stack.Install(destRtr);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer srcRtr1Ifaces = address.Assign(srcRtr1Devs);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer srcRtr2Ifaces = address.Assign(srcRtr2Devs);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr1DestRtrIfaces = address.Assign(rtr1DestRtrDevs);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr2DestRtrIfaces = address.Assign(rtr2DestRtrDevs);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer destRtrDestIfaces = address.Assign(destRtrDestDevs);

    // Configure static routing
    Ipv4StaticRoutingHelper staticRouting;

    // Configure routing for Rtr1
    Ptr<Node> router1 = rtr1.Get(0);
    Ptr<Ipv4StaticRouting> routingTable1 = staticRouting.GetStaticRouting(router1->GetObject<Ipv4>());
    routingTable1->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), srcRtr1Ifaces.GetAddress(1), 1); // Through Rtr1

    // Configure routing for Rtr2
    Ptr<Node> router2 = rtr2.Get(0);
    Ptr<Ipv4StaticRouting> routingTable2 = staticRouting.GetStaticRouting(router2->GetObject<Ipv4>());
    routingTable2->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), srcRtr2Ifaces.GetAddress(1), 1); // Through Rtr2

    // Configure routing for DestRtr
    Ptr<Node> destinationRouter = destRtr.Get(0);
    Ptr<Ipv4StaticRouting> routingTableDestRtr = staticRouting.GetStaticRouting(destinationRouter->GetObject<Ipv4>());
    routingTableDestRtr->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), rtr1DestRtrIfaces.GetAddress(1), 1); //Rtr1 Path
    routingTableDestRtr->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), rtr2DestRtrIfaces.GetAddress(1), 1); //Rtr2 Path


    // Set default gateway for srcNode and destNode
    Ptr<Node> sourceNode = srcNode.Get(0);
    Ptr<Ipv4StaticRouting> routingTableSrc = staticRouting.GetStaticRouting(sourceNode->GetObject<Ipv4>());
    routingTableSrc->SetDefaultRoute(srcRtr1Ifaces.GetAddress(1), 1);

    Ptr<Node> destinationNode = destNode.Get(0);
    Ptr<Ipv4StaticRouting> routingTableDest = staticRouting.GetStaticRouting(destinationNode->GetObject<Ipv4>());
    routingTableDest->SetDefaultRoute(destRtrDestIfaces.GetAddress(0), 1);

    // Create TCP application (Client and Server)
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(destNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create OnOff application
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    ApplicationContainer clientApp = onOffHelper.Install(srcNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));


    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}