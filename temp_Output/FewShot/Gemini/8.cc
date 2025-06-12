#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t nSpokes = 3;

    NodeContainer spokes;
    spokes.Create(nSpokes);

    NodeContainer hub;
    hub.Create(1);

    NodeContainer allNodes;
    allNodes.Add(hub);
    allNodes.Add(spokes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer spokeDevices;
    NetDeviceContainer hubDevices;

    for (uint32_t i = 0; i < nSpokes; ++i) {
        NetDeviceContainer link = pointToPoint.Install(hub.Get(0), spokes.Get(i));
        spokeDevices.Add(link.Get(1));
        hubDevices.Add(link.Get(0));
    }

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer spokeInterfaces = address.Assign(spokeDevices);
    Ipv4InterfaceContainer hubInterfaces = address.Assign(hubDevices);

    uint16_t port = 9;

    for (uint32_t i = 0; i < nSpokes; ++i) {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(spokes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));
    }

    UdpEchoClientHelper echoClient0(spokeInterfaces.GetAddress(0), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp0 = echoClient0.Install(spokes.Get(1));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(spokeInterfaces.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(spokes.Get(2));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(spokeInterfaces.GetAddress(2), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(spokes.Get(0));
    clientApp2.Start(Seconds(4.0));
    clientApp2.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim("star-network.xml");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}