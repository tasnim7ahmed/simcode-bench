#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarNetwork");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[5];
    for (int i = 0; i < 5; ++i) {
        devices[i] = pointToPoint.Install(nodes.Get(0), nodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces[5];
    for (int i = 0; i < 5; ++i) {
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    uint16_t echoPort = 9;

    for (int i = 1; i < 5; ++i) {
        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(1), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(i * 2.0));
        clientApps.Stop(Seconds(i * 2.0 + 5.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim("star-network.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
    anim.SetConstantPosition(nodes.Get(2), 0.0, 20.0);
    anim.SetConstantPosition(nodes.Get(3), 20.0, 0.0);
    anim.SetConstantPosition(nodes.Get(4), 0.0, 0.0);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}