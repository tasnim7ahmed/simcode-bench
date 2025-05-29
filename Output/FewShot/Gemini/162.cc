#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices30 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);
    Ipv4InterfaceContainer interfaces30 = address.Assign(devices30);

    uint16_t port = 50000;
    TcpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    TcpClientHelper client(interfaces23.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("ring-persistent-tcp.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 0.0, 10.0);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}