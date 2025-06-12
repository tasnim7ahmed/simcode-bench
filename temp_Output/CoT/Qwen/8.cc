#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;
    double simulationTime = 10.0;
    std::string animFile = "star_topology.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes to place in the star.", numNodes);
    cmd.AddValue("simTime", "Simulation time in seconds.", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer edgeNodes;
    edgeNodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer centralDevices;
    NetDeviceContainer edgeDevices;

    for (uint32_t i = 0; i < numNodes; ++i) {
        NetDeviceContainer link = p2p.Install(centralNode.Get(0), edgeNodes.Get(i));
        centralDevices.Add(link.Get(0));
        edgeDevices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer centralInterfaces;
    Ipv4InterfaceContainer edgeInterfaces;

    for (uint32_t i = 0; i < numNodes; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(centralDevices.Get(i), edgeDevices.Get(i)));
        centralInterfaces.Add(interfaces.Get(0));
        edgeInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    ApplicationContainer serverApps;
    UdpEchoServerHelper echoServer(9);
    for (uint32_t i = 0; i < numNodes; ++i) {
        serverApps.Add(echoServer.Install(edgeNodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    Time packetInterval = Seconds(1.0);
    for (uint32_t clientIndex = 0; clientIndex < numNodes; ++clientIndex) {
        UdpEchoClientHelper echoClient(edgeInterfaces.Get(clientIndex).GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(packetInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(centralNode.Get(0));
        clientApp.Start(Seconds(1.0 + clientIndex * 2.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    AnimationInterface anim(animFile);
    anim.SetConstantPosition(centralNode.Get(0), 0.0, 0.0);
    for (uint32_t i = 0; i < numNodes; ++i) {
        double angle = 2 * M_PI / numNodes * i;
        double x = 10.0 * cos(angle);
        double y = 10.0 * sin(angle);
        anim.SetConstantPosition(edgeNodes.Get(i), x, y);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}