#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 1 AP and 3 STAs
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(3);

    // Create Wi-Fi channel and PHY
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set Wi-Fi standard to 802.11a for AarfWifiManager compatibility
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Install MAC layer
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::ApWifiMac"); // AP MAC
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac"); // STA MAC
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    // Mobility models
    MobilityHelper mobility;

    // Set constant position for AP
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    // Set random walk for STAs
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Distance", DoubleValue(5.0),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5, Max=1.5]"));
    mobility.Install(staNodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Setup UDP server on AP
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP client on one of the stations (e.g., first STA)
    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(300));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(staNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable tracing
    wifiPhy.EnablePcapAll("wifi_udp_simulation");

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}