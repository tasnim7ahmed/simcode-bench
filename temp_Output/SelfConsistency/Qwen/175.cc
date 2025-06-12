#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiThreeNodeSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 1 AP and 2 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Combine all nodes for mobility and visualization
    NodeContainer allNodes = NodeContainer(wifiApNode, wifiStaNodes);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz); // Using 802.11n in 5 GHz band

    // Set up PHY and channel
    YansWifiPhyHelper phy;
    phy.SetChannelWidth(20); // MHz
    phy.Set("TxPowerStart", DoubleValue(15.0));
    phy.Set("TxPowerEnd", DoubleValue(15.0));
    phy.Set("TxGain", DoubleValue(0));
    phy.Set("RxGain", DoubleValue(0));
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up MAC layer
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-three-node-network");

    // Configure the AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)),
                "EnableBeaconJitter", BooleanValue(false));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configure the STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Positioning nodes using MobilityHelper
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Set up applications: UDP Echo client-server between STA1 and STA2 via AP
    uint16_t port = 9; // Echo port

    // Server on STA2 (node 1)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on STA1 (node 0), sends packets to STA2
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable NetAnim visualization
    AnimationInterface anim("wifi_three_node_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Optional: color nodes in animation
    anim.UpdateNodeDescription(wifiApNode.Get(0), "AP");
    anim.UpdateNodeDescription(wifiStaNodes.Get(0), "STA1");
    anim.UpdateNodeDescription(wifiStaNodes.Get(1), "STA2");

    anim.UpdateNodeColor(wifiApNode.Get(0), 255, 0, 0);     // Red for AP
    anim.UpdateNodeColor(wifiStaNodes.Get(0), 0, 255, 0);   // Green for STA1
    anim.UpdateNodeColor(wifiStaNodes.Get(1), 0, 0, 255);   // Blue for STA2

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}