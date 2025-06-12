#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;
    uint32_t packetCount = 4;
    Time interPacketInterval = Seconds(1.0);
    DataRate linkRate("5Mbps");
    Time linkDelay("2ms");

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer edgeNodes;
    edgeNodes.Create(numNodes - 1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(linkRate));
    p2p.SetChannelAttribute("Delay", TimeValue(linkDelay));

    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    for (uint32_t i = 0; i < edgeNodes.GetN(); ++i) {
        NetDeviceContainer link = p2p.Install(NodeContainer(centralNode, edgeNodes.Get(i)));
        Ipv4InterfaceContainer ifc = address.Assign(link);
        interfaces.Add(ifc);
        address.NewNetwork();
    }

    ApplicationContainer serverApps;
    UdpEchoServerHelper echoServer(9);
    serverApps = echoServer.Install(centralNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < edgeNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(2 * i + 1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer app = echoClient.Install(edgeNodes.Get(i));
        app.Start(Seconds(1.0 + i * (packetCount * interPacketInterval.GetSeconds() + 1)));
        app.Stop(Seconds(10.0));
    }

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("star-network.tr"));

    AnimationInterface anim("star-network.xml");
    anim.SetConstantPosition(centralNode.Get(0), 0.0, 0.0);
    double angleStep = (2 * M_PI) / (numNodes - 1);
    for (uint32_t i = 0; i < edgeNodes.GetN(); ++i) {
        double x = 10.0 * cos(i * angleStep);
        double y = 10.0 * sin(i * angleStep);
        anim.SetConstantPosition(edgeNodes.Get(i), x, y);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}