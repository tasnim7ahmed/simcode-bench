#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulationExample");

int main(int argc, char *argv[])
{
    // Set simulation time
    double simulationTime = 10.0;

    // Create 2 access points and 4 stations (STAs)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    // Configure the Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi helper and configure for 802.11n standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("AP1");
    Ssid ssid2 = Ssid("AP2");

    // Install Wi-Fi devices on AP nodes (Access Points)
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer ap1Devices = wifi.Install(phy, mac, wifiApNodes.Get(0));
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer ap2Devices = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Install Wi-Fi devices on STA nodes (Stations)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes.Get(0));
    staDevices1.Add(wifi.Install(phy, mac, wifiStaNodes.Get(1)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, wifiStaNodes.Get(2));
    staDevices2.Add(wifi.Install(phy, mac, wifiStaNodes.Get(3)));

    // Set up mobility model for APs (fixed)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // Set up mobility for STAs (random)
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1Interfaces = address.Assign(ap1Devices);
    Ipv4InterfaceContainer sta1Interfaces = address.Assign(staDevices1);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ap2Interfaces = address.Assign(ap2Devices);
    Ipv4InterfaceContainer sta2Interfaces = address.Assign(staDevices2);

    // Create UDP traffic application
    uint16_t port = 9; // Discard port
    UdpEchoServerHelper echoServer(port);

    // Install server on STA 1 and STA 3
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Add(echoServer.Install(wifiStaNodes.Get(2)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Install clients on STA 2 and STA 4 to send packets to STA 1 and STA 3
    UdpEchoClientHelper echoClient(sta1Interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(1));
    echoClient.SetAttribute("RemoteAddress", AddressValue(sta2Interfaces.GetAddress(0)));
    clientApps.Add(echoClient.Install(wifiStaNodes.Get(3)));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable NetAnim
    AnimationInterface anim("wifi_netanim.xml");

    // Set positions for visualization
    anim.SetConstantPosition(wifiApNodes.Get(0), 0, 0);    // AP1 at origin
    anim.SetConstantPosition(wifiApNodes.Get(1), 10, 0);   // AP2 at (10,0)
    anim.SetConstantPosition(wifiStaNodes.Get(0), -5, -5); // STA 1
    anim.SetConstantPosition(wifiStaNodes.Get(1), 5, -5);  // STA 2
    anim.SetConstantPosition(wifiStaNodes.Get(2), -5, 5);  // STA 3
    anim.SetConstantPosition(wifiStaNodes.Get(3), 5, 5);   // STA 4

    anim.EnablePacketMetadata(true); // Enable packet metadata for tracking

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

