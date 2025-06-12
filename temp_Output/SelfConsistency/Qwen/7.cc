#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpEcho");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for the ring topology
    uint32_t nNodes = 4;
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Create point-to-point links between adjacent nodes to form a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[nNodes];
    for (uint32_t i = 0; i < nNodes; ++i) {
        uint32_t j = (i + 1) % nNodes;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[nNodes];

    for (uint32_t i = 0; i < nNodes; ++i) {
        std::ostringstream subnet;
        subnet << "192.9.39." << i * 64;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up UDP Echo Server and Client Applications
    uint16_t port = 9;

    // Each node acts as both client and server once
    for (uint32_t i = 0; i < nNodes; ++i) {
        uint32_t serverIndex = (i + 1) % nNodes; // Next node in the ring

        // Install server on serverNode
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(serverIndex));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Install client on node i
        UdpEchoClientHelper echoClient(interfaces[serverIndex].GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0 + i * 3.0)); // staggered start times
        clientApps.Stop(Seconds(10.0));
    }

    // Enable animation
    AnimationInterface anim("ring-topology-udp-echo.xml");
    for (uint32_t i = 0; i < nNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0); // Red color for all nodes
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}