#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWifi11pExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for 3 vehicles
    NodeContainer nodes;
    nodes.Create(3);

    // Setup mobility model: constant velocity along X-axis
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Assign different velocities to each node
    DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>())->SetVelocity(Vector(20.0, 0.0, 0.0));
    DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(1)->GetObject<MobilityModel>())->SetVelocity(Vector(15.0, 0.0, 0.0));
    DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(2)->GetObject<MobilityModel>())->SetVelocity(Vector(10.0, 0.0, 0.0));

    // Install WAVE/802.11p devices
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211_2016);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6_5MbpsBW10MHz"), "ControlMode", StringValue("OfdmRate6_5MbpsBW10MHz"));

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    WaveDeviceHelper waveHelper = WaveDeviceHelper::Default();
    waveHelper.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("100p")));

    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, nodes);

    // Set up IP addresses
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP Echo Server on the last vehicle (node 2)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Client on the first vehicle (node 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}