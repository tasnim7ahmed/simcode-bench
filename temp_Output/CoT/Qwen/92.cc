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

    NodeContainer source;
    source.Create(1);

    NodeContainer routers;
    routers.Create(3); // Rtr1, Rtr2, DestRtr

    NodeContainer destination;
    destination.Create(1);

    PointToPointHelper p2pSourceRtr1;
    p2pSourceRtr1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pSourceRtr1.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pSourceRtr2;
    p2pSourceRtr2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pSourceRtr2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pRtr1DestRtr;
    p2pRtr1DestRtr.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRtr1DestRtr.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2pRtr2DestRtr;
    p2pRtr2DestRtr.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRtr2DestRtr.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2pDestRtrDestination;
    p2pDestRtrDestination.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pDestRtrDestination.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer sourceDevicesRtr1;
    NetDeviceContainer sourceDevicesRtr2;
    NetDeviceContainer rtr1DevicesDestRtr;
    NetDeviceContainer rtr2DevicesDestRtr;
    NetDeviceContainer destRtrDevicesDestination;

    sourceDevicesRtr1 = p2pSourceRtr1.Install(source.Get(0), routers.Get(0));
    sourceDevicesRtr2 = p2pSourceRtr2.Install(source.Get(0), routers.Get(1));
    rtr1DevicesDestRtr = p2pRtr1DestRtr.Install(routers.Get(0), routers.Get(2));
    rtr2DevicesDestRtr = p2pRtr2DestRtr.Install(routers.Get(1), routers.Get(2));
    destRtrDevicesDestination = p2pDestRtrDestination.Install(routers.Get(2), destination.Get(0));

    InternetStackHelper stack;
    stack.Install(source);
    stack.Install(routers);
    stack.Install(destination);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRtr1Interfaces = address.Assign(sourceDevicesRtr1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRtr2Interfaces = address.Assign(sourceDevicesRtr2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr1DestRtrInterfaces = address.Assign(rtr1DevicesDestRtr);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rtr2DestRtrInterfaces = address.Assign(rtr2DevicesDestRtr);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer destRtrDestinationInterfaces = address.Assign(destRtrDevicesDestination);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Manually add static routes
    Ptr<Ipv4> ipv4Rtr1 = routers.Get(0)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrRtr1 = ipv4Rtr1->GetAddress(1, 0);
    Ipv4Address ipRtr1 = ifAddrRtr1.GetLocal();

    Ptr<Ipv4> ipv4Rtr2 = routers.Get(1)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrRtr2 = ipv4Rtr2->GetAddress(1, 0);
    Ipv4Address ipRtr2 = ifAddrRtr2.GetLocal();

    Ptr<Ipv4> ipv4DestRtr = routers.Get(2)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrDestRtr = ipv4DestRtr->GetAddress(2, 0);
    Ipv4Address ipDestRtr = ifAddrDestRtr.GetLocal();

    Ipv4Address destIp = destRtrDestinationInterfaces.GetAddress(1);

    // Route from Rtr1 to destination via DestRtr
    ipv4Rtr1->GetRoutingProtocol()->GetObject<Ipv4StaticRouting>()->AddHostRouteTo(destIp, ipDestRtr, 1);

    // Route from Rtr2 to destination via DestRtr
    ipv4Rtr2->GetRoutingProtocol()->GetObject<Ipv4StaticRouting>()->AddHostRouteTo(destIp, ipDestRtr, 1);

    // Route from Source to destination via Rtr1 for first connection
    ipv4Rtr1->GetRoutingProtocol()->GetObject<Ipv4StaticRouting>()->AddHostRouteTo(destIp, sourceRtr1Interfaces.GetAddress(1), 0);

    // Route from Source to destination via Rtr2 for second connection
    ipv4Rtr2->GetRoutingProtocol()->GetObject<Ipv4StaticRouting>()->AddHostRouteTo(destIp, sourceRtr2Interfaces.GetAddress(1), 0);

    uint16_t sinkPort = 8080;

    Address sinkAddress(InetSocketAddress(destRtrDestinationInterfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(destination.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // First TCP connection: via Rtr1
    Ptr<Socket> socket1 = Socket::CreateSocket(source.Get(0), TcpSocketFactory::GetTypeId());
    socket1->Connect(Address(sinkAddress));
    socket1->Send(Create<Packet>(1024));
    Simulator::Schedule(Seconds(1.0), &Socket::Send, socket1, Create<Packet>(1024));

    // Second TCP connection: via Rtr2
    Ptr<Socket> socket2 = Socket::CreateSocket(source.Get(0), TcpSocketFactory::GetTypeId());
    socket2->Bind();
    socket2->Connect(Address(sinkAddress));
    socket2->Send(Create<Packet>(1024));
    Simulator::Schedule(Seconds(2.0), &Socket::Send, socket2, Create<Packet>(1024));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}