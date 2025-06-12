#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetworkExample");

// Test 1: Node Creation (AP and STA)
void TestNodeCreation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Verify that 1 AP node and 2 STA nodes are created
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Expected 2 STA nodes");
    NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Expected 1 AP node");
}

// Test 2: Wi-Fi Device Installation (STA and AP)
void TestWifiDeviceInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Install devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Verify device installation
    NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Expected 2 STA devices");
    NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Expected 1 AP device");
}

// Test 3: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Install devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install the Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to STA node 1");
    NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Failed to assign IP to STA node 2");
    NS_TEST_ASSERT_MSG_NE(apInterface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to AP node");
}

// Test 4: Mobility Model Configuration (AP and STA)
void TestMobilityConfiguration()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    MobilityHelper mobility;

    // Set up station mobility
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Set up AP mobility (static)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Verify that mobility models are set up
    Ptr<MobilityModel> staMobility = wifiStaNodes.Get(0)->GetObject<MobilityModel>();
    NS_TEST_ASSERT_MSG_NE(staMobility, nullptr, "STA mobility model not set up correctly");

    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    NS_TEST_ASSERT_MSG_NE(apMobility, nullptr, "AP mobility model not set up correctly");
}

// Test 5: Application Installation (UDP client and server)
void TestApplicationInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Install devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install the Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Set up UDP applications on STA nodes
    uint16_t port = 9;
    UdpClientHelper udpClient(apInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Set up UDP server on the AP
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Verify application installation
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 2, "Failed to install UDP client on both STA nodes");
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on AP node");
}

// Test 6: Packet Reception (Check UDP traffic)
void TestPacketReception()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Install devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install the Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Set up UDP applications on STA nodes
    uint16_t port = 9;
    UdpClientHelper udpClient(apInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Set up UDP server on the AP
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();

    // Verify packet reception
    Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
    NS_TEST_ASSERT_MSG_GT(server->GetReceived(), 0, "AP did not receive any packets");

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestIpAddressAssignment();
    TestMobilityConfiguration();
    TestApplicationInstallation();
    TestPacketReception();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
