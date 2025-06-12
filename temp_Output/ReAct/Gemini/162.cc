#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    interfaces[0] = address.Assign(devices[0]);
    interfaces[1] = address.Assign(devices[1]);
    interfaces[2] = address.Assign(devices[2]);
    interfaces[3] = address.Assign(devices[3]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    // Server (node 3)
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Client (node 0)
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->SetConnectTimeout(Seconds(60));

    TypeId tid = TypeId::LookupByName("ns3::TcpSocketMsg");
    Ptr<TcpSocketMsg> msgSocket = DynamicCast<TcpSocketMsg>(ns3TcpSocket);
    msgSocket->SetNoDelay(true);

    InetSocketAddress remoteAddress(interfaces[3].GetAddress(0), port);
    msgSocket->Connect(remoteAddress);

    BulkSendHelper source("ns3::TcpSocketMsgFactory", InetSocketAddress(interfaces[3].GetAddress(0), port));
    source.SetAttribute("SendSize", UintegerValue(1024));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(2.0));
    sourceApp.Stop(Seconds(9.0));

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("ring-persistent-tcp.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 10, 0);
    anim.SetConstantPosition(nodes.Get(2), 20, 0);
    anim.SetConstantPosition(nodes.Get(3), 30, 0);

    pointToPoint.EnablePcapAll("ring-persistent-tcp");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}