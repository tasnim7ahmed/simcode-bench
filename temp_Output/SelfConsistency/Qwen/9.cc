#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpEcho");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and Server
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between consecutive nodes to form a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t j = (i + 1) % 4;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "192.9.39." << i * 64 << "/24";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Setup UDP echo server and client applications
    uint16_t port = 9;

    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t serverNodeIndex = i;
        uint32_t clientNodeIndex = (i + 1) % 4;

        // Setup server application
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(serverNodeIndex));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Setup client application
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(clientNodeIndex));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        port++;
    }

    // Set up mobility for NetAnim visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Enable NetAnim visualization
    AnimationInterface anim("ring-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 20.0, 0.0);
    anim.SetConstantPosition(nodes.Get(3), 30.0, 0.0);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}