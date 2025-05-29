#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a TCP echo server application
    uint16_t port = 9; // well-known echo port number
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Create a TCP echo client application
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<Ipv4> ipv4 = nodes.Get(1)->GetObject<Ipv4>();
    Ipv4Address serverAddress = ipv4->GetAddress(1, 0).GetLocal();

    TypeId tid = TypeId::LookupByName("ns3::TcpSocketMsg");
    Ptr<TcpSocketMsg> tcpSocket = DynamicCast<TcpSocketMsg>(ns3TcpSocket);
    tcpSocket->SetAttribute("MaxPacketCount", UintegerValue(1));
    tcpSocket->SetAttribute("MessageSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    Ptr<TcpClient> app = CreateObject<TcpClient>();
    app->Setup(ns3TcpSocket, InetSocketAddress(serverAddress, port), 1024);
    nodes.Get(0)->AddApplication(app);
    clientApps.Add(app);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Configure NetAnim
    AnimationInterface anim("tcp-non-persistent.xml");
    anim.SetConstantPosition(nodes.Get(0), 1, 1);
    anim.SetConstantPosition(nodes.Get(1), 2, 2);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}