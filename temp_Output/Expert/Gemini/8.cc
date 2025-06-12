#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t nSpokes = 4;
    double startTime = 1.0;
    double interPacketInterval = 0.5;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 4;

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer spokeNodes;
    spokeNodes.Create(nSpokes);

    NodeContainer allNodes;
    allNodes.Add(hubNode);
    allNodes.Add(spokeNodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer hubInterface;
    Ipv4InterfaceContainer spokeInterface[nSpokes];

    for (uint32_t i = 0; i < nSpokes; ++i) {
        NetDeviceContainer devices = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));
        hubInterface.Add(address.AssignOne(devices.Get(0)));
        spokeInterface[i].Add(address.AssignOne(devices.Get(1)));
        address.NewNetwork();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps[nSpokes];
    for (uint32_t i = 0; i < nSpokes; ++i) {
        serverApps[i] = echoServer.Install(spokeNodes.Get(i));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(10.0));
    }


    UdpEchoClientHelper echoClient(spokeInterface[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nSpokes; ++i) {
        UdpEchoClientHelper currentClient(spokeInterface[i].GetAddress(0), 9);
        currentClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
        currentClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        currentClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApps = currentClient.Install(spokeNodes.Get((i + 1) % nSpokes));
        clientApps.Start(Seconds(startTime + i * (numPackets * interPacketInterval + 1.0)));
        clientApps.Stop(Seconds(10.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(hubNode.Get(0), 0, 0);
    for (uint32_t i = 0; i < nSpokes; ++i) {
        anim.SetConstantPosition(spokeNodes.Get(i), 5 * cos(2 * M_PI * i / nSpokes), 5 * sin(2 * M_PI * i / nSpokes));
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}