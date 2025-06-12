#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: one vehicle and one RSU
    NodeContainer nodes;
    nodes.Create(2);
    Ptr<Node> vehicle = nodes.Get(0);
    Ptr<Node> rsu = nodes.Get(1);

    // Setup WAVE (Wi-Fi 802.11p) PHY & MAC
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Set 802.11p standard
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211_2016);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("OfdmRate6_5MbpsBW10MHz"),
                                  "ControlMode", StringValue("OfdmRate6_5MbpsBW10MHz"));

    // Install MAC and PHY on both nodes
    NetDeviceContainer devices;
    mac.SetType("ns3::WaveBsmMac");
    devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicle);

    // RSU is stationary
    MobilityHelper rsuMobility;
    rsuMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    rsuMobility.Install(rsu);

    // Start vehicle at (0,0) moving along x-axis at 20 m/s
    vehicle->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    vehicle->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));

    // RSU located at (100, 0)
    rsu->GetObject<MobilityModel>()->SetPosition(Vector(100.0, 0.0, 0.0));

    // Setup UDP echo server on RSU
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(rsu);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo client on vehicle
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(vehicle);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("vanet_simulation", devices);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}