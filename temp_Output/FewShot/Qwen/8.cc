#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Parse command line arguments
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create a central node and peripheral nodes
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);  // Four outer nodes

    // Combine all nodes for internet stack installation
    NodeContainer allNodes = NodeContainer(centralNode, peripheralNodes);

    // Point-to-point helper configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point links between central and each peripheral node
    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        NodeContainer pair = NodeContainer(centralNode.Get(0), peripheralNodes.Get(i));
        devices[i] = p2p.Install(pair);
    }

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces[4];
    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "192.9.39." << (i * 2) << "/24";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Install UDP Echo Servers on all peripheral nodes
    ApplicationContainer serverApps[4];
    uint16_t port = 9;
    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoServerHelper echoServer(port);
        serverApps[i] = echoServer.Install(peripheralNodes.Get(i));
        serverApps[i].Start(Seconds(1.0));
        serverApps[i].Stop(Seconds(20.0));
    }

    // Install UDP Echo Clients on the central node to communicate with each peripheral node in sequence
    ApplicationContainer clientApps[4];
    Time startTime = Seconds(2.0);
    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        clientApps[i] = echoClient.Install(centralNode.Get(0));
        clientApps[i].Start(startTime);
        clientApps[i].Stop(startTime + Seconds(5.0));
        startTime += Seconds(5.0);
    }

    // Set up animation output
    AnimationInterface anim("star_topology_udp_echo.xml");
    anim.SetConstantPosition(centralNode.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(peripheralNodes.Get(0), -10.0, 10.0);
    anim.SetConstantPosition(peripheralNodes.Get(1), 10.0, 10.0);
    anim.SetConstantPosition(peripheralNodes.Get(2), 10.0, -10.0);
    anim.SetConstantPosition(peripheralNodes.Get(3), -10.0, -10.0);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}