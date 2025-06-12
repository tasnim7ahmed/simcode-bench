#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"
#include "ns3/energy-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 5 sensors + 1 sink = 6 nodes
    NodeContainer sensorNodes;
    sensorNodes.Create(5);
    NodeContainer sinkNode;
    sinkNode.Create(1);

    // Create channel using LR-WPAN (802.15.4)
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devices;

    // Connect all sensor nodes and sink to the same channel
    devices = lrWpanHelper.Install(sensorNodes);
    devices.Add(lrWpanHelper.Install(sinkNode));

    // Set mobility for star topology
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(6),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer::GetGlobal());

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(NodeContainer::GetGlobal());

    // Assign addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP server on sink node (node 0)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(sinkNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP clients on sensor nodes
    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        clientApps.Add(client.Install(sensorNodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Animation
    AnimationInterface anim("sensor-network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Energy model not included here for simplicity but can be added if needed

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}