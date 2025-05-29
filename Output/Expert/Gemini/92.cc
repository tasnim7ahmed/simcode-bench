#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer srcNode, destNode, rtr1, rtr2, destRtr;
    srcNode.Create(1);
    destNode.Create(1);
    rtr1.Create(1);
    rtr2.Create(1);
    destRtr.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer srcRtr1Devs = p2p.Install(srcNode.Get(0), rtr1.Get(0));
    NetDeviceContainer srcRtr2Devs = p2p.Install(srcNode.Get(0), rtr2.Get(0));
    NetDeviceContainer rtr1DestRtrDevs = p2p.Install(rtr1.Get(0), destRtr.Get(0));
    NetDeviceContainer rtr2DestRtrDevs = p2p.Install(rtr2.Get(0), destRtr.Get(0));
    NetDeviceContainer destRtrDestDevs = p2p.Install(destRtr.Get(0), destNode.Get(0));

    InternetStackHelper stack;
    stack.Install(srcNode);
    stack.Install(destNode);
    stack.Install(rtr1);
    stack.Install(rtr2);
    stack.Install(destRtr);

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

    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> rtr1StaticRouting = staticRouting.GetStaticRouting(rtr1.Get(0)->GetObject<Ipv4>());
    rtr1StaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.2"), 1);
    Ptr<Ipv4StaticRouting> rtr2StaticRouting = staticRouting.GetStaticRouting(rtr2.Get(0)->GetObject<Ipv4>());
    rtr2StaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.4.2"), 1);

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(destNode.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(srcNode.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->Bind();
    ns3TcpSocket->Connect(InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));

    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("tcp-trace.txt", std::ios::out);
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", "TcpCwndTracer", stream);

    Ptr<Packet> packet = Create<Packet>(1024);

    Simulator::ScheduleWithContext(ns3TcpSocket->GetNode()->GetId(), Seconds(2.0), &Socket::Send, ns3TcpSocket, packet);
    Simulator::ScheduleWithContext(ns3TcpSocket->GetNode()->GetId(), Seconds(4.0), &Socket::Send, ns3TcpSocket, packet);
    Simulator::ScheduleWithContext(ns3TcpSocket->GetNode()->GetId(), Seconds(6.0), &Socket::Send, ns3TcpSocket, packet);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}