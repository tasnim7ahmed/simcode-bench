#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    uint32_t nNodes = 5; // total number of nodes in the star (1 central + others)
    uint32_t nClients = nNodes - 1;
    uint32_t nPackets = 4;
    DataRate linkRate("5Mbps");
    Time linkDelay = MilliSeconds(2);

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer edgeNodes;
    edgeNodes.Create(nClients);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(linkRate));
    p2p.SetChannelAttribute("Delay", TimeValue(linkDelay));

    NetDeviceContainer centralDevices;
    NetDeviceContainer edgeDevices;

    for (uint32_t i = 0; i < nClients; ++i) {
        NetDeviceContainer container = p2p.Install(centralNode.Get(0), edgeNodes.Get(i));
        centralDevices.Add(container.Get(0));
        edgeDevices.Add(container.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer centralInterfaces;
    Ipv4InterfaceContainer edgeInterfaces;

    for (uint32_t i = 0; i < nClients; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(centralDevices.Get(i), edgeDevices.Get(i)));
        centralInterfaces.Add(interfaces.Get(0));
        edgeInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(centralNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    double startTime = 2.0;
    for (uint32_t i = 0; i < nClients; ++i) {
        UdpEchoClientHelper echoClient(centralInterfaces.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(edgeNodes.Get(i));
        clientApps.Start(Seconds(startTime));
        clientApps.Stop(Seconds(startTime + 4.0));
        startTime += 5.0; // staggered start to ensure sequential communication
    }

    AnimationInterface anim("star_topology.xml");
    anim.SetConstantPosition(centralNode.Get(0), 0.0, 0.0);
    double angleStep = (2 * M_PI) / nClients;
    for (uint32_t i = 0; i < nClients; ++i) {
        double angle = i * angleStep;
        double x = 10.0 * cos(angle);
        double y = 10.0 * sin(angle);
        anim.SetConstantPosition(edgeNodes.Get(i), x, y);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}