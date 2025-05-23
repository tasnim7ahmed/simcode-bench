#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main()
{
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DropTailQueue", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(5));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(2000));
    client.SetAttribute("Interval", TimeValue(NanoSeconds(100000)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    NetAnimHelper netanim;
    netanim.EnableAnimation("packet_loss_netanim.xml");
    nodes.Get(0)->AggregateObject(CreateObject<NetAnimScenarioDescription::NodeDescription>()->SetPosition(10.0, 10.0, 0.0));
    nodes.Get(1)->AggregateObject(CreateObject<NetAnimScenarioDescription::NodeDescription>()->SetPosition(50.0, 10.0, 0.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}