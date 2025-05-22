#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetworkExample");

// Test 1: Node creation (Access Points and Stations)
void TestNodeCreation()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(4);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(4);

    NS_TEST_ASSERT_MSG_EQ(wifiApNodes.GetN(), 2, "Expected 2 Access Points");
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes1.GetN(), 4, "Expected 4 stations for AP 1");
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes2.GetN(), 4, "Expected 4 stations for AP 2");
}

// Test 2: Wi-Fi channel and PHY configuration
void TestWifiChannelAndPhy()
{
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    NS_TEST_ASSERT_MSG_NE(channel.GetChannel(), nullptr, "Failed to create Wi-Fi channel");
    NS_TEST_ASSERT_MSG_NE(phy.GetChannel(), nullptr, "Failed to create PHY");
}

// Test 3: Mobility model installation
void TestMobilityInstallation()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(4);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(4);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNodes);
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);

    Ptr<ConstantPositionMobilityModel> mobilityModelAp = wifiApNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mobilityModelSta = wifiStaNodes1.Get(0)->GetObject<ConstantPositionMobilityModel>();

    NS_TEST_ASSERT_MSG_NE(mobilityModelAp, nullptr, "Mobility model not installed for AP 1");
    NS_TEST_ASSERT_MSG_NE(mobilityModelSta, nullptr, "Mobility model not installed for station 1");
}

// Test 4: IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(4);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(4);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    Ssid ssid1 = Ssid("network-1");
    Ssid ssid2 = Ssid("network-2");

    // AP Devices
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Station Devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes1);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, wifiStaNodes2);

    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes1);
    stack.Install(wifiStaNodes2);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer ap1Interface, ap2Interface, sta1Interface, sta2Interface;

    address.SetBase("10.1.1.0", "255.255.255.0");
    ap1Interface = address.Assign(apDevices1);
    sta1Interface = address.Assign(staDevices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    ap2Interface = address.Assign(apDevices2);
    sta2Interface = address.Assign(staDevices2);

    // Check if IP addresses are correctly assigned
    NS_TEST_ASSERT_MSG_NE(ap1Interface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to AP 1");
    NS_TEST_ASSERT_MSG_NE(sta1Interface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to Station 1");
}

// Test 5: Application installation (UDP Server and Clients)
void TestApplicationInstallation()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(4);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(4);

    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp1 = udpServer.Install(wifiApNodes.Get(0));
    ApplicationContainer serverApp2 = udpServer.Install(wifiApNodes.Get(1));

    serverApp1.Start(Seconds(1.0));
    serverApp2.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(10.0));
    serverApp2.Stop(Seconds(10.0));

    UdpClientHelper udpClient1(Ipv4Address("10.1.1.1"), 9);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient1.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(1024));

    UdpClientHelper udpClient2(Ipv4Address("10.1.2.1"), 9);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = udpClient1.Install(wifiStaNodes1);
    ApplicationContainer clientApps2 = udpClient2.Install(wifiStaNodes2);

    clientApps1.Start(Seconds(2.0));
    clientApps2.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));
    clientApps2.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp1.Get(0)->GetStartTime().GetSeconds(), 1.0, "Server 1 did not start at the correct time");
    NS_TEST_ASSERT_MSG_EQ(serverApp2.Get(0)->GetStartTime().GetSeconds(), 1.0, "Server 2 did not start at the correct time");
}

// Test 6: Packet transmission verification
void TestPacketTransmission()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(4);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(4);

    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp1 = udpServer.Install(wifiApNodes.Get(0));
    ApplicationContainer serverApp2 = udpServer.Install(wifiApNodes.Get(1));

    serverApp1.Start(Seconds(1.0));
    serverApp2.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(10.0));
    serverApp2.Stop(Seconds(10.0));

    UdpClientHelper udpClient1(Ipv4Address("10.1.1.1"), 9);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient1.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(1024));

    UdpClientHelper udpClient2(Ipv4Address("10.1.2.1"), 9);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = udpClient1.Install(wifiStaNodes1);
    ApplicationContainer clientApps2 = udpClient2.Install(wifiStaNodes2);

    clientApps1.Start(Seconds(2.0));
    clientApps2.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));
    clientApps2.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    Ptr<UdpServer> server1 = DynamicCast<UdpServer>(serverApp1.Get(0));
    Ptr<UdpServer> server2 = DynamicCast<UdpServer>(serverApp2.Get(0));

    NS_TEST_ASSERT_MSG_GT(server1->GetReceived(), 0, "AP 1 did not receive any packets");
    NS_TEST_ASSERT_MSG_GT(server2->GetReceived(), 0, "AP 2 did not receive any packets");

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestWifiChannelAndPhy();
    TestMobilityInstallation();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestPacketTransmission();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
