#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"  // Include the ns3 Test module for unit testing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulationExampleTest");

// Test the creation of Wi-Fi nodes (APs and STAs)
void TestNodeCreation() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 4, "Incorrect number of STA nodes created");
    NS_TEST_ASSERT_MSG_EQ(wifiApNodes.GetN(), 2, "Incorrect number of AP nodes created");
}

// Test Wi-Fi device installation on APs and STAs
void TestWifiDeviceInstallation() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("AP1");
    Ssid ssid2 = Ssid("AP2");

    // Install Wi-Fi devices on AP nodes
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer ap1Devices = wifi.Install(phy, mac, wifiApNodes.Get(0));
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer ap2Devices = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Install Wi-Fi devices on STA nodes
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes.Get(0));
    staDevices1.Add(wifi.Install(phy, mac, wifiStaNodes.Get(1)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, wifiStaNodes.Get(2));
    staDevices2.Add(wifi.Install(phy, mac, wifiStaNodes.Get(3)));

    // Verify Wi-Fi devices installation
    NS_TEST_ASSERT_MSG_EQ(ap1Devices.GetN(), 1, "Incorrect number of AP1 devices installed");
    NS_TEST_ASSERT_MSG_EQ(ap2Devices.GetN(), 1, "Incorrect number of AP2 devices installed");
    NS_TEST_ASSERT_MSG_EQ(staDevices1.GetN(), 2, "Incorrect number of STA devices for SSID 1 installed");
    NS_TEST_ASSERT_MSG_EQ(staDevices2.GetN(), 2, "Incorrect number of STA devices for SSID 2 installed");
}

// Test mobility setup (fixed APs and random STAs)
void TestMobilitySetup() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes); // Fixed mobility for APs

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes); // Random mobility for STAs

    // Verify mobility models for APs and STAs
    Ptr<ConstantPositionMobilityModel> ap1Mobility = wifiApNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> ap2Mobility = wifiApNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    Ptr<RandomWalk2dMobilityModel> sta1Mobility = wifiStaNodes.Get(0)->GetObject<RandomWalk2dMobilityModel>();

    NS_TEST_ASSERT_MSG_NE(ap1Mobility, nullptr, "AP1 mobility model not installed correctly");
    NS_TEST_ASSERT_MSG_NE(ap2Mobility, nullptr, "AP2 mobility model not installed correctly");
    NS_TEST_ASSERT_MSG_NE(sta1Mobility, nullptr, "STA1 mobility model not installed correctly");
}

// Test IP address assignment for APs and STAs
void TestIpAddressAssignment() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("AP1");
    Ssid ssid2 = Ssid("AP2");

    // Install Wi-Fi devices on APs and STAs
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer ap1Devices = wifi.Install(phy, mac, wifiApNodes.Get(0));
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer ap2Devices = wifi.Install(phy, mac, wifiApNodes.Get(1));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes.Get(0));
    staDevices1.Add(wifi.Install(phy, mac, wifiStaNodes.Get(1)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, wifiStaNodes.Get(2));
    staDevices2.Add(wifi.Install(phy, mac, wifiStaNodes.Get(3)));

    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1Interfaces = address.Assign(ap1Devices);
    Ipv4InterfaceContainer sta1Interfaces = address.Assign(staDevices1);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ap2Interfaces = address.Assign(ap2Devices);
    Ipv4InterfaceContainer sta2Interfaces = address.Assign(staDevices2);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_NE(sta1Interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "STA1 IP address not assigned correctly");
    NS_TEST_ASSERT_MSG_NE(sta2Interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "STA2 IP address not assigned correctly");
}

// Test application installation (UDP Echo Server and Client)
void TestApplicationInstallation() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    uint16_t port = 9; // Discard port
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Add(echoServer.Install(wifiStaNodes.Get(2)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv4Address("192.168.1.1"), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(1));
    echoClient.SetAttribute("RemoteAddress", AddressValue(Ipv4Address("192.168.2.1")));
    clientApps.Add(echoClient.Install(wifiStaNodes.Get(3)));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Verify application installation
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 2, "UDP Echo Server not installed correctly");
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 2, "UDP Echo Client not installed correctly");
}

// Test NetAnim visualization setup
void TestNetAnimVisualization() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    AnimationInterface anim("wifi_netanim_test.xml");

    anim.SetConstantPosition(wifiApNodes.Get(0), 0, 0);    // AP1 at origin
    anim.SetConstantPosition(wifiApNodes.Get(1), 10, 0);   // AP2 at (10,0)
    anim.SetConstantPosition(wifiStaNodes.Get(0), -5, -5); // STA 1
    anim.SetConstantPosition(wifiStaNodes.Get(1), 5, -5);  // STA 2
    anim.SetConstantPosition(wifiStaNodes.Get(2), -5, 5);  // STA 3
    anim.SetConstantPosition(wifiStaNodes.Get(3), 5, 5);   // STA 4

    anim.EnablePacketMetadata(true); // Enable packet metadata for tracking

    // Verify NetAnim file creation and positions
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "wifi_netanim_test.xml", "NetAnim file was not created correctly");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(wifiApNodes.Get(0)).x, 0.0, "AP1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(wifiStaNodes.Get(1)).y, -5.0, "STA2 position incorrect");
}

int main() {
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestMobilitySetup();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestNetAnimVisualization();

    return 0;
}
