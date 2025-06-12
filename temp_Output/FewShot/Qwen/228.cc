#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for DSR
    LogComponentEnable("DsrRouting", LOG_LEVEL_INFO);
    LogComponentEnable("DsrHelper", LOG_LEVEL_INFO);

    // Create nodes (representing vehicles)
    NodeContainer nodes;
    nodes.Create(10);  // Simulate 10 vehicles

    // Install Internet stack with DSR routing protocol
    DsrStackHelper dsr;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsr); // Assign DSR as the routing protocol
    stack.Install(nodes);

    // Create Wi-Fi channel and devices
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for VANET
    wifiPhy.SetChannel(wifiPhy.CreateChannel());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Mobility model using SUMO trace file
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Configure SUMO integration to load vehicle traces
    std::string sumoConfigFile = "vanet.sumocfg"; // Assume this file exists in the working directory
    Config::SetDefault("ns3::SumoMobilityHelper::SumoConfig", StringValue(sumoConfigFile));
    Config::SetDefault("ns3::SumoMobilityHelper::SumoBinary", StringValue("sumo"));
    Config::SetDefault("ns3::SumoMobilityHelper::MaxX", DoubleValue(2000.0));
    Config::SetDefault("ns3::SumoMobilityHelper::MaxY", DoubleValue(2000.0));

    SumoMobilityHelper sumoHelper;
    sumoHelper.Install(nodes);

    // Setup UDP echo applications for communication
    uint16_t port = 9;
    UdpEchoServerHelper serverApp(port);
    ApplicationContainer serverApps = serverApp.Install(nodes.Get(9)); // Last node as server
    serverApps.Start(Seconds(10.0));
    serverApps.Stop(Seconds(80.0));

    UdpEchoClientHelper clientApp(interfaces.GetAddress(9), port);
    clientApp.SetAttribute("MaxPackets", UintegerValue(100));
    clientApp.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientApp.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = clientApp.Install(nodes.Get(0)); // First node as client
    clientApps.Start(Seconds(20.0));
    clientApps.Stop(Seconds(80.0));

    // Enable packet tracing
    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("vanet-dsr.tr"));

    // Animate simulation
    AnimationInterface anim("vanet-dsr.xml");
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(80.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}