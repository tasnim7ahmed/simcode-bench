#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWifiSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for three vehicles
    NodeContainer nodes;
    nodes.Create(3);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Assign different velocities along X-axis
    Ptr<ConstantVelocityMobilityModel> velModel1 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    velModel1->SetVelocity(Vector(20.0, 0.0, 0.0)); // Vehicle 0 at 20 m/s

    Ptr<ConstantVelocityMobilityModel> velModel2 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    velModel2->SetVelocity(Vector(15.0, 0.0, 0.0)); // Vehicle 1 at 15 m/s

    Ptr<ConstantVelocityMobilityModel> velModel3 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    velModel3->SetVelocity(Vector(10.0, 0.0, 0.0)); // Vehicle 2 at 10 m/s

    // Install WAVE (Wi-Fi 802.11p) devices
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211_2016);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6_5MbpsBW10MHz"), "ControlMode", StringValue("OfdmRate6_5MbpsBW10MHz"));

    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    WifiMacHelper wifiMac = mac.Create();

    WaveNetDeviceHelper waveHelper;
    NetDeviceContainer devices = waveHelper.Install(nodes, wifiPhy, wifiMac);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications
    uint16_t port = 9; // UDP echo server port

    // Server on node 2
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Client on node 0
    UdpEchoClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(3));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation duration
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}