#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5; // Total nodes in the star topology
    uint32_t packetCount = 4; // Each client sends 4 packets
    Time interPacketInterval = Seconds(1.0); // Interval between packets
    DataRate linkRate("5Mbps");
    Time linkDelay("2ms");

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the star topology.", numNodes);
    cmd.AddValue("packetCount", "Number of packets each client sends.", packetCount);
    cmd.AddValue("interval", "Time interval between packets.", interPacketInterval);
    cmd.Parse(argc, argv);

    // Enable logging for UdpEchoClient and Server
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create the central node (hub)
    Ptr<Node> centralNode = CreateObject<Node>();

    // Create peripheral nodes
    NodeContainer peripheralNodes;
    peripheralNodes.Create(numNodes - 1);

    // Combine all nodes into a container
    NodeContainer allNodes(centralNode, peripheralNodes);

    // Create point-to-point helper for links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(linkRate));
    p2p.SetChannelAttribute("Delay", TimeValue(linkDelay));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    NetDeviceContainer centralDevices;
    NetDeviceContainer peripheralDevices;

    // Connect each peripheral node to the central node
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
        NetDeviceContainer link = p2p.Install(centralNode, peripheralNodes.Get(i));
        centralDevices.Add(link.Get(0));
        peripheralDevices.Add(link.Get(1));
        address.NewNetwork();
        address.Assign(link);
    }

    // Set up UDP echo server on all nodes
    UdpEchoServerHelper echoServer(9); // Port 9 for echo service

    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
        serverApps.Add(echoServer.Install(allNodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Schedule one client-server communication at a time
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
        // Client is peripheral node i, server is central node
        UdpEchoClientHelper echoClient(centralNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(peripheralNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 2.0)); // Staggered start times
        clientApp.Stop(Seconds(10.0));
    }

    // Setup animation
    AnimationInterface anim("star_topology_udp_echo.xml");
    anim.SetConstantPosition(centralNode, 0, 0); // Central node at center
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
        double angle = (2 * M_PI * i) / (numNodes - 1);
        double radius = 100.0;
        double x = radius * cos(angle);
        double y = radius * sin(angle);
        anim.SetConstantPosition(peripheralNodes.Get(i), x, y);
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}