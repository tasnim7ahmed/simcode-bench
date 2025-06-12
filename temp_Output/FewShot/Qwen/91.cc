#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceRoutingSimulation");

void ReceivePacketAtSource(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(From))) {
        NS_LOG_INFO("Source received packet of size " << packet->GetSize() << " from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
}

void ReceivePacketAtDestination(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(From))) {
        NS_LOG_INFO("Destination received packet of size " << packet->GetSize() << " from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("MultiInterfaceRoutingSimulation", LOG_LEVEL_INFO);

    NodeContainer source, router1, router2, destination;
    source.Create(1);
    router1.Create(1);
    router2.Create(1);
    destination.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Source to Router1
    NetDeviceContainer sourceRouter1 = p2p.Install(NodeContainer(source, router1));

    // Source to Router2
    NetDeviceContainer sourceRouter2 = p2p.Install(NodeContainer(source, router2));

    // Router1 to Destination
    NetDeviceContainer router1Dest = p2p.Install(NodeContainer(router1, destination));

    // Router2 to Destination
    NetDeviceContainer router2Dest = p2p.Install(NodeContainer(router2, destination));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sourceRouter1Ifaces = address.Assign(sourceRouter1);
    address.NewNetwork();
    Ipv4InterfaceContainer sourceRouter2Ifaces = address.Assign(sourceRouter2);
    address.NewNetwork();
    Ipv4InterfaceContainer router1DestIfaces = address.Assign(router1Dest);
    address.NewNetwork();
    Ipv4InterfaceContainer router2DestIfaces = address.Assign(router2Dest);

    Ipv4StaticRoutingHelper routingHelper;

    // Static routes for Router1 to Destination
    Ptr<Ipv4StaticRouting> router1StaticRouting = routingHelper.GetStaticRouting(router1.Get(0)->GetObject<Ipv4>());
    router1StaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), 2, 10);

    // Static routes for Router2 to Destination
    Ptr<Ipv4StaticRouting> router2StaticRouting = routingHelper.GetStaticRouting(router2.Get(0)->GetObject<Ipv4>());
    router2StaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.4.0"), Ipv4Mask("255.255.255.0"), 2, 20); // Higher metric

    // Static routes for Source to Destination via both routers
    Ptr<Ipv4StaticRouting> sourceStaticRouting = routingHelper.GetStaticRouting(source.Get(0)->GetObject<Ipv4>());
    sourceStaticRouting->AddHostRouteTo(Ipv4Address("10.1.3.2"), sourceRouter1Ifaces.GetAddress(1), 1, 10); // Lower metric
    sourceStaticRouting->AddHostRouteTo(Ipv4Address("10.1.4.2"), sourceRouter2Ifaces.GetAddress(1), 2, 20); // Higher metric

    // Static route on Destination back through Router1
    Ipv4StaticRoutingHelper().AddHostRouteTo(destination.Get(0), Ipv4Address("10.1.1.1"), router1DestIfaces.GetAddress(1), 1);
    // Static route on Destination back through Router2
    Ipv4StaticRoutingHelper().AddHostRouteTo(destination.Get(0), Ipv4Address("10.1.2.1"), router2DestIfaces.GetAddress(1), 2);

    uint16_t destPort = 9;
    Address destAddress(InetSocketAddress(router1DestIfaces.GetAddress(1), destPort));
    Address destAddress2(InetSocketAddress(router2DestIfaces.GetAddress(1), destPort));

    // Server application on destination
    UdpEchoServerHelper echoServer(destPort);
    ApplicationContainer serverApp = echoServer.Install(destination);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Client application on source with multiple interfaces
    UdpEchoClientHelper echoClient(destAddress);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(source);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Bind second set of packets using a socket directly
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(source.Get(0), tid);
    socket->Bind();
    socket->Connect(destAddress2);

    Simulator::ScheduleWithContext(socket->GetNode()->GetId(), Seconds(3.0),
                                   &Socket::Send, socket, Create<Packet>(1024));
    Simulator::ScheduleWithContext(socket->GetNode()->GetId(), Seconds(4.0),
                                   &Socket::Send, socket, Create<Packet>(1024));

    // Setup receive callbacks
    Ptr<Socket> sourceSocket = Socket::CreateSocket(source.Get(0), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), destPort);
    sourceSocket->Bind(local);
    sourceSocket->SetRecvCallback(MakeCallback(&ReceivePacketAtSource));

    Ptr<Socket> destSocket = Socket::CreateSocket(destination.Get(0), tid);
    destSocket->Bind(local);
    destSocket->SetRecvCallback(MakeCallback(&ReceivePacketAtDestination));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}