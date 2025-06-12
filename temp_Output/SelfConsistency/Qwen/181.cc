#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable calculation of rssi (received signal strength)
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2000"));

    // Create nodes for two vehicles
    NodeContainer nodes;
    nodes.Create(2);

    // Setup mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Start with initial positions and set constant velocities
    Ptr<ConstantVelocityMobilityModel> node0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    node0->SetPosition(Vector(0, 0, 0));
    node0->SetVelocity(Vector(20, 0, 0));  // moving east at 20 m/s

    Ptr<ConstantVelocityMobilityModel> node1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    node1->SetPosition(Vector(50, 0, 0));  // start 50 meters ahead
    node1->SetVelocity(Vector(15, 0, 0));  // moving slower at 15 m/s

    // Configure DSRC/WAVE using Wifi in ad-hoc mode
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211p);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                       "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP communication from node 0 to node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Animation
    AsciiTraceHelper asciiTraceHelper;
    MobilityHelper::EnableAsciiAll(asciiTraceHelper.CreateFileStream("vanet.mobility.tr"));
    AnimationInterface anim("vanet-animation.xml");

    // Simulation time
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}