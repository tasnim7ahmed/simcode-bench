#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t nLeaf = 4; // Number of peripheral nodes
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer leafNodes;
    leafNodes.Create(nLeaf);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    for (uint32_t i = 0; i < nLeaf; ++i) {
        allNodes.Add(leafNodes.Get(i));
    }

    // Create point-to-point links between central node and each leaf node
    std::vector<NetDeviceContainer> deviceContainers(nLeaf);
    std::vector<NodeContainer> starLinks(nLeaf);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 0; i < nLeaf; ++i) {
        NodeContainer link;
        link.Add(centralNode.Get(0));
        link.Add(leafNodes.Get(i));
        starLinks[i] = link;
        deviceContainers[i] = pointToPoint.Install(link);
    }

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses: 192.9.39.0/24
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    std::vector<Ipv4InterfaceContainer> interfaces(nLeaf);
    uint32_t ipSuffix = 1;

    for (uint32_t i = 0; i < nLeaf; ++i) {
        std::ostringstream subnet;
        subnet << "192.9.39." << 1 + 2 * i << "0";
        std::string base = subnet.str().substr(0, subnet.str().length() - 1) + "0";
        address.SetBase(base.c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(deviceContainers[i]);
        address.NewNetwork();
    }

    // Set fixed positions for visualization
    AnimationInterface anim("star-ring-simulation.xml");
    anim.SetConstantPosition(centralNode.Get(0), 50, 50);
    double radius = 40.0;
    for (uint32_t i = 0; i < nLeaf; ++i) {
        double angle = (2 * M_PI * i) / nLeaf;
        double x = 50 + radius * std::cos(angle);
        double y = 50 + radius * std::sin(angle);
        anim.SetConstantPosition(leafNodes.Get(i), x, y);
    }

    // Port base for EchoServer/EchoClient
    uint16_t portBase = 9000;

    // For each node (both leaf and central), install UDP echo server
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nLeaf; ++i) {
        UdpEchoServerHelper echoServer(portBase + i);
        ApplicationContainer serverApp = echoServer.Install(leafNodes.Get(i));
        serverApp.Start(Seconds(0.1));
        serverApp.Stop(Seconds(40.0));
        serverApps.Add(serverApp);
    }
    UdpEchoServerHelper echoServerCentral(portBase + nLeaf);
    ApplicationContainer serverAppCentral = echoServerCentral.Install(centralNode.Get(0));
    serverAppCentral.Start(Seconds(0.1));
    serverAppCentral.Stop(Seconds(40.0));
    serverApps.Add(serverAppCentral);

    // Sequentially schedule client-server pairs (ring-like)
    // Central <-> Leaf0, Leaf0 <-> Leaf1, Leaf1 <-> Leaf2, ..., LastLeaf <-> Central

    // Central node sends to Leaf 0
    double appStart = 1.0;
    double duration = 2.5;
    {
        UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), portBase + 0);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.3)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = echoClient.Install(centralNode.Get(0));
        clientApp.Start(Seconds(appStart));
        clientApp.Stop(Seconds(appStart + duration));
    }
    appStart += duration;

    // Leaf 0 sends to Leaf 1
    {
        UdpEchoClientHelper echoClient(interfaces[1].GetAddress(1), portBase + 1);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.3)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = echoClient.Install(leafNodes.Get(0));
        clientApp.Start(Seconds(appStart));
        clientApp.Stop(Seconds(appStart + duration));
    }
    appStart += duration;

    // Leaf 1 sends to Leaf 2
    if (nLeaf >= 3) {
        UdpEchoClientHelper echoClient(interfaces[2].GetAddress(1), portBase + 2);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.3)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = echoClient.Install(leafNodes.Get(1));
        clientApp.Start(Seconds(appStart));
        clientApp.Stop(Seconds(appStart + duration));
        appStart += duration;
    }

    // Last Leaf sends to Central node
    {
        UdpEchoClientHelper echoClient(interfaces[nLeaf-1].GetAddress(0), portBase + nLeaf);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.3)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = echoClient.Install(leafNodes.Get(nLeaf-1));
        clientApp.Start(Seconds(appStart));
        clientApp.Stop(Seconds(appStart + duration));
    }

    Simulator::Stop(Seconds(appStart + duration + 1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}