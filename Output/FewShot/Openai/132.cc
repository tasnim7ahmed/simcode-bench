#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DvTwoNodeLoop");

void
PrintRoutingTable(Ptr<Node> node, Time printTime)
{
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>(&std::cout);
    rp->Print(routingStream, printTime.GetSeconds());
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // 0: A, 1: B, 2: Dest

    // A <-> B,  B <-> Dest, A <-> Dest
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: A-B, A-Dest, B-Dest
    NetDeviceContainer ab = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer ad = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer bd = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.SetRoutingHelper(Ipv4ListRoutingHelper()
        .Add(Ipv4GlobalRoutingHelper(), 0).Add(Ipv4StaticRoutingHelper(), 10));
    stack.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_ab = ipv4.Assign(ab);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_ad = ipv4.Assign(ad);
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_bd = ipv4.Assign(bd);

    // Set up UDP echo server on Dest (node 2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Set up UDP echo client on A to Dest
    UdpEchoClientHelper echoClient(i_ad.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Print routing tables before failure
    Simulator::Schedule(Seconds(1.5), &PrintRoutingTable, nodes.Get(0), Seconds(1.5));
    Simulator::Schedule(Seconds(1.5), &PrintRoutingTable, nodes.Get(1), Seconds(1.5));

    // Simulate failure of A-Dest link at t=5s (simulate that dest becomes unreachable from A)
    Simulator::Schedule(Seconds(5.0), [&](){
        ab.Get(0)->SetReceiveErrorModel(CreateObject<RateErrorModel>());
        ab.Get(1)->SetReceiveErrorModel(CreateObject<RateErrorModel>());
        ad.Get(0)->SetReceiveErrorModel(CreateObject<RateErrorModel>(true));
        ad.Get(1)->SetReceiveErrorModel(CreateObject<RateErrorModel>(true));
        bd.Get(0)->SetReceiveErrorModel(CreateObject<RateErrorModel>(true));
        bd.Get(1)->SetReceiveErrorModel(CreateObject<RateErrorModel>(true));
        // Only A-B link remains, but remove A-Dest and B-Dest
        ad.Get(0)->SetReceiveErrorModel(CreateObject<RateErrorModel>());
        ad.Get(1)->SetReceiveErrorModel(CreateObject<RateErrorModel>());
        bd.Get(0)->SetReceiveErrorModel(CreateObject<RateErrorModel>());
        bd.Get(1)->SetReceiveErrorModel(CreateObject<RateErrorModel>());
        std::cout << "**** Simulated failure: (A|B)-Dest links removed at t=5s ****" << std::endl;
    });

    // Remove Dest links from A&B so only A-B exists, forcing them to try to route via each other
    Simulator::Schedule(Seconds(5.0), [&](){
        Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();

        Ptr<Ipv4StaticRouting> routingA = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ipv4A->GetRoutingProtocol());
        Ptr<Ipv4StaticRouting> routingB = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ipv4B->GetRoutingProtocol());

        routingA->RemoveRoute(1); // Remove route to Dest via i_ad
        routingB->RemoveRoute(1); // Remove route to Dest via i_bd

        std::cout << "**** Removed static routes to Dest at t=5s for nodes A and B ****" << std::endl;
    });

    // Print routing tables after failure
    Simulator::Schedule(Seconds(6.0), &PrintRoutingTable, nodes.Get(0), Seconds(6.0));
    Simulator::Schedule(Seconds(6.0), &PrintRoutingTable, nodes.Get(1), Seconds(6.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}