#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(3);

    // Set up Wi-Fi 802.11p (WAVE) PHY and MAC
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211_2016);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    // Install WAVE MAC and PHY on all nodes
    WaveMacHelper waveMac = WaveMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(waveMac, wifiPhy, vehicles);

    // Set up constant velocity mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Assign positions and velocities
    Ptr<ConstantVelocityMobilityModel> velModel1 =
        vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    velModel1->SetPosition(Vector(0.0, 0.0, 0.0));
    velModel1->SetVelocity(Vector(20.0, 0.0, 0.0));  // 20 m/s along X-axis

    Ptr<ConstantVelocityMobilityModel> velModel2 =
        vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    velModel2->SetPosition(Vector(50.0, 0.0, 0.0));
    velModel2->SetVelocity(Vector(15.0, 0.0, 0.0));  // 15 m/s along X-axis

    Ptr<ConstantVelocityMobilityModel> velModel3 =
        vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    velModel3->SetPosition(Vector(100.0, 0.0, 0.0));
    velModel3->SetVelocity(Vector(10.0, 0.0, 0.0));  // 10 m/s along X-axis

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses in the 192.168.1.0/24 subnet
    Ipv4AddressHelper ip;
    ip.SetBase("192.168.1.0", "255.255.255.0");
    ip.Assign(devices);

    // Set up UDP Echo Server on the third vehicle (node index 2)
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on the first vehicle (node index 0)
    UdpEchoClientHelper echoClient(Ipv4Address("192.168.1.3"), port);  // Server IP is third node
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}