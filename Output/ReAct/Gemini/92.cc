#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(6);

    NodeContainer sourceNode, destNode, rtr1Node, rtr2Node, destRtrNode;
    sourceNode.Add(nodes.Get(0));
    rtr1Node.Add(nodes.Get(1));
    rtr2Node.Add(nodes.Get(2));
    destRtrNode.Add(nodes.Get(3));
    destNode.Add(nodes.Get(4));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer srcRtr1Devs = p2p.Install(sourceNode, rtr1Node);
    NetDeviceContainer srcRtr2Devs = p2p.Install(sourceNode, rtr2Node);
    NetDeviceContainer rtr1DestRtrDevs = p2p.Install(rtr1Node, destRtrNode);
    NetDeviceContainer rtr2DestRtrDevs = p2p.Install(rtr2Node, destRtrNode);
    NetDeviceContainer destRtrDestDevs = p2p.Install(destRtrNode, destNode);

    InternetStackHelper stack;
    stack.Install(nodes);

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

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Static routing configuration
    Ptr<Ipv4StaticRouting> staticRoutingRtr1 = Ipv4RoutingHelper::GetStaticRouting(rtr1Node.Get(0)->GetObject<Ipv4>());
    staticRoutingRtr1->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), rtr1DestRtrIfaces.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> staticRoutingRtr2 = Ipv4RoutingHelper::GetStaticRouting(rtr2Node.Get(0)->GetObject<Ipv4>());
    staticRoutingRtr2->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), rtr2DestRtrIfaces.GetAddress(1), 1);

     Ptr<Ipv4StaticRouting> staticRoutingDestRtr = Ipv4RoutingHelper::GetStaticRouting(destRtrNode.Get(0)->GetObject<Ipv4>());
    staticRoutingDestRtr->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), rtr1DestRtrIfaces.GetAddress(0), 1);
    staticRoutingDestRtr->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), rtr2DestRtrIfaces.GetAddress(0), 1);

    uint16_t port = 50000;

    // Define the server (destination) application.
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));
    ApplicationContainer sinkApp = sinkHelper.Install(destNode);
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Define the client (source) application that sends via Rtr1
    BulkSendHelper bulkSendHelper1("ns3::TcpSocketFactory", InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));
    bulkSendHelper1.SetAttribute("SendSize", UintegerValue(1024));
    bulkSendHelper1.SetAttribute("MaxBytes", UintegerValue(1000000));

    ApplicationContainer sourceApp1 = bulkSendHelper1.Install(sourceNode);
    sourceApp1.Start(Seconds(2.0));
    sourceApp1.Stop(Seconds(5.0));

     // Define the client (source) application that sends via Rtr2
    BulkSendHelper bulkSendHelper2("ns3::TcpSocketFactory", InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));
    bulkSendHelper2.SetAttribute("SendSize", UintegerValue(1024));
    bulkSendHelper2.SetAttribute("MaxBytes", UintegerValue(1000000));

    ApplicationContainer sourceApp2 = bulkSendHelper2.Install(sourceNode);
    sourceApp2.Start(Seconds(6.0));
    sourceApp2.Stop(Seconds(9.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}