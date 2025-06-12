#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetUdpDsrSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for the simulation
    NodeContainer nodes;
    nodes.Create(10);  // Simulate 10 vehicles

    // Setup Wi-Fi for vehicular communication
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    WifiHelper wifi;
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack with DSR routing
    InternetStackHelper stack;
    DsrRoutingHelper dsrRouting;
    stack.SetRoutingHelper(dsrRouting);  // Use DSR for dynamic source routing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up SUMO mobility using MobilityHelper
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Enable animation if desired
    AnimationInterface anim("vanet_udp_dsr_simulation.xml");
    anim.EnablePacketMetadata(true);

    // Set up UDP echo application as an example traffic generator
    uint16_t port = 9;   // Use standard echo port
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(80.0));

    UdpEchoClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(80.0));

    // Simulation setup complete
    Simulator::Stop(Seconds(80.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}