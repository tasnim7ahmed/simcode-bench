#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/energy-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    double radius = 50.0; // Radius of the circle (meters)
    uint32_t numNodes = 6;
    Time simulationTime = Seconds(10);
    uint16_t port = 9;
    Time interval = Seconds(1.0);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install LR-WPAN devices (IEEE 802.15.4)
    LrWpanHelper lrWpan;
    NetDeviceContainer lrWpanDevices = lrWpan.Install(nodes);

    // Assign fixed positions in a circular topology
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes; ++i) {
        double angle = 2 * M_PI * i / numNodes;
        double x = radius * cos(angle);
        double y = radius * sin(angle);
        positionAlloc->Add(Vector(x, y, 0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Enable logging
    LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Install Internet stack with IPv4
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign addresses
    Ipv4AddressHelper ipv4;
    NS_ABORT_MSG_UNLESS(numNodes <= 253, "Too many nodes for /24 subnet");
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(lrWpanDevices);

    // Setup UDP server on sink node (node 0)
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    // Setup UDP clients on other nodes
    for (uint32_t i = 1; i < numNodes; ++i) {
        UdpClientHelper client(interfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(interval));
        client.SetAttribute("PacketSize", UintegerValue(128));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0 + i)); // staggered start times
        clientApp.Stop(simulationTime);
    }

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup NetAnim for visualization
    AnimationInterface anim("wsn-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));
    for (uint32_t i = 0; i < numNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0); // Red color
    }
    anim.UpdateNodeColor(nodes.Get(0), 0, 255, 0); // Green for sink

    // Run simulation
    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}