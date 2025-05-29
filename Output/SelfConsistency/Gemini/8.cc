#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopology");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer hubNode;
    hubNode.Add(nodes.Get(0));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        NodeContainer spokeNode;
        spokeNode.Add(nodes.Get(i));
        NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), spokeNode.Get(0));
        hubDevices.Add(link.Get(0));
        spokeDevices.Add(link.Get(1));
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices, spokeDevices));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
    anim.SetConstantPosition(nodes.Get(2), 30.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 20.0, 0.0);
    anim.SetConstantPosition(nodes.Get(4), 30.0, 30.0);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}