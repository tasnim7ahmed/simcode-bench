#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5); // 0:source, 1:Rtr1, 2:Rtr2, 3:DestRouter, 4:destination

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create point-to-point links and assign subnets
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer sourceToRtr1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer sourceToRtr2 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer rtr1ToDestRtr = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer rtr2ToDestRtr = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer destRtrToDest = p2p.Install(nodes.Get(3), nodes.Get(4));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRtr1Ifaces = ipv4.Assign(sourceToRtr1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRtr2Ifaces = ipv4.Assign(sourceToRtr2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr1DestRtrIfaces = ipv4.Assign(rtr1ToDestRtr);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr2DestRtrIfaces = ipv4.Assign(rtr2ToDestRtr);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer destRtrDestIfaces = ipv4.Assign(destRtrToDest);

    // Set up static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Explicit routes via Rtr1 (10.1.3.x) or Rtr2 (10.1.4.x)
    Ptr<Ipv4StaticRouting> routeSourceViaRtr1 = Ipv4StaticRouting::GetRouting(nodes.Get(0)->GetObject<Ipv4>());
    routeSourceViaRtr1->AddHostRouteTo(destRtrDestIfaces.GetAddress(1), sourceRtr1Ifaces.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> routeSourceViaRtr2 = Ipv4StaticRouting::GetRouting(nodes.Get(0)->GetObject<Ipv4>());
    routeSourceViaRtr2->AddHostRouteTo(destRtrDestIfaces.GetAddress(1), sourceRtr2Ifaces.GetAddress(1), 2);

    // Sink application on destination node
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(destRtrDestIfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(4));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP socket from source to destination using different paths
    Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    // Send first packet via Rtr1
    Simulator::Schedule(Seconds(1.0), &Socket::Connect, tcpSocket, sinkAddress);
    Simulator::Schedule(Seconds(1.0), &Socket::Send, tcpSocket, Create<Packet>(1024), 0);

    // Send second packet via Rtr2 by setting the local interface explicitly
    Simulator::Schedule(Seconds(2.0), [&]() {
        tcpSocket->Bind(sourceRtr2Ifaces.GetAddress(0));
        tcpSocket->Connect(sinkAddress);
        tcpSocket->Send(Create<Packet>(1024));
    });

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}