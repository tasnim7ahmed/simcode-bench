#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    Node *source = nodes.Get(0);
    Node *rtr1 = nodes.Get(1);
    Node *rtr2 = nodes.Get(2);
    Node *rtr3 = nodes.Get(3);
    Node *destination = nodes.Get(4);
    Node *sinkNode = nodes.Get(5);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer srcRtr1Devs = p2p.Install(source, rtr1);
    NetDeviceContainer srcRtr2Devs = p2p.Install(source, rtr2);
    NetDeviceContainer rtr1Rtr3Devs = p2p.Install(rtr1, rtr3);
    NetDeviceContainer rtr2Rtr3Devs = p2p.Install(rtr2, rtr3);
    NetDeviceContainer rtr3DestDevs = p2p.Install(rtr3, destination);
    NetDeviceContainer destSinkDevs = p2p.Install(destination, sinkNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer srcRtr1Ifaces = address.Assign(srcRtr1Devs);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer srcRtr2Ifaces = address.Assign(srcRtr2Devs);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr1Rtr3Ifaces = address.Assign(rtr1Rtr3Devs);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr2Rtr3Ifaces = address.Assign(rtr2Rtr3Devs);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr3DestIfaces = address.Assign(rtr3DestDevs);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer destSinkIfaces = address.Assign(destSinkDevs);

    // Set up static routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<Ipv4StaticRouting> staticRoutingRtr1 = Ipv4RoutingHelper::GetStaticRouting(rtr1->GetObject<Ipv4>());
    staticRoutingRtr1->AddHostRouteToNetwork(Ipv4Address("10.1.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.2"), 1);

    Ptr<Ipv4StaticRouting> staticRoutingRtr2 = Ipv4RoutingHelper::GetStaticRouting(rtr2->GetObject<Ipv4>());
    staticRoutingRtr2->AddHostRouteToNetwork(Ipv4Address("10.1.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.4.2"), 1);

    Ptr<Ipv4StaticRouting> staticRoutingRtr3 = Ipv4RoutingHelper::GetStaticRouting(rtr3->GetObject<Ipv4>());
    staticRoutingRtr3->AddHostRouteToNetwork(Ipv4Address("10.1.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.5.2"), 1);

    Ptr<Ipv4StaticRouting> staticRoutingSource = Ipv4RoutingHelper::GetStaticRouting(source->GetObject<Ipv4>());
    staticRoutingSource->AddHostRouteToNetwork(Ipv4Address("10.1.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.1.2"), 1);
    staticRoutingSource->AddHostRouteToNetwork(Ipv4Address("10.1.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.2.2"), 1);

    // Create sink application
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(sinkNode);
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create OnOff application to send traffic via Rtr1
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(destSinkIfaces.GetAddress(1), port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer clientApp1 = onOffHelper.Install(source);
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(5.0));

    //Create OnOff application to send traffic via Rtr2
    OnOffHelper onOffHelper2("ns3::TcpSocketFactory", InetSocketAddress(destSinkIfaces.GetAddress(1), port));
    onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper2.SetAttribute("DataRate", StringValue("1Mbps"));
    ApplicationContainer clientApp2 = onOffHelper2.Install(source);
    clientApp2.Start(Seconds(6.0));
    clientApp2.Stop(Seconds(9.0));

    // Set source routing for specific paths
    Ptr<Socket> socket1 = Socket::CreateSocket(source, TcpSocketFactory::GetTypeId());
    socket1->Bind();
    socket1->Connect(InetSocketAddress(destSinkIfaces.GetAddress(1), port));
    Ptr<Ipv4> ipv4 = source->GetObject<Ipv4>();
    uint32_t interfaceIndexRtr1 = ipv4->GetInterfaceForAddress(srcRtr1Ifaces.GetAddress(0))->GetIfIndex();
    socket1->SetAttribute("Ipv4RoutingProtocol", StringValue("ns3::Ipv4StaticRouting"));
    socket1->SetAttribute("Ipv4Interface", UintegerValue(interfaceIndexRtr1));

    Ptr<Socket> socket2 = Socket::CreateSocket(source, TcpSocketFactory::GetTypeId());
    socket2->Bind();
    socket2->Connect(InetSocketAddress(destSinkIfaces.GetAddress(1), port));
    ipv4 = source->GetObject<Ipv4>();
    uint32_t interfaceIndexRtr2 = ipv4->GetInterfaceForAddress(srcRtr2Ifaces.GetAddress(0))->GetIfIndex();
    socket2->SetAttribute("Ipv4RoutingProtocol", StringValue("ns3::Ipv4StaticRouting"));
    socket2->SetAttribute("Ipv4Interface", UintegerValue(interfaceIndexRtr2));

    // Enable PCAP tracing
    p2p.EnablePcapAll("multi-interface-static-routing");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}