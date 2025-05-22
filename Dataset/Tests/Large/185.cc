#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Test for the creation of Wi-Fi AP and STA nodes
void TestNodeCreation()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(3); // 3 Access Points

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(6); // 6 Stations

    NS_TEST_ASSERT_MSG_EQ(wifiApNodes.GetN(), 3, "Failed to create 3 Access Point nodes");
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 6, "Failed to create 6 Station nodes");
}

// Test for Wi-Fi device installation on APs
void TestApDeviceInstallation()
{
    WifiHelper wifiHelper;
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());
    
    WifiMacHelper macHelper;
    Ssid ssid1 = Ssid("AP1-SSID");
    NodeContainer wifiApNodes;
    wifiApNodes.Create(3); // 3 Access Points

    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(0));
    NetDeviceContainer apDevices2 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(1));
    NetDeviceContainer apDevices3 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(2));

    NS_TEST_ASSERT_MSG_EQ(apDevices1.GetN(), 1, "Failed to install Wi-Fi device on AP 1");
    NS_TEST_ASSERT_MSG_EQ(apDevices2.GetN(), 1, "Failed to install Wi-Fi device on AP 2");
    NS_TEST_ASSERT_MSG_EQ(apDevices3.GetN(), 1, "Failed to install Wi-Fi device on AP 3");
}

// Test for Wi-Fi device installation on STA nodes
void TestStaDeviceInstallation()
{
    WifiHelper wifiHelper;
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiMacHelper macHelper;
    Ssid ssid1 = Ssid("AP1-SSID");
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(6); // 6 Stations

    NetDeviceContainer staDevices1 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(0));
    staDevices1.Add(wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(1)));

    NetDeviceContainer staDevices2 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(2));
    staDevices2.Add(wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(3)));

    NetDeviceContainer staDevices3 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(4));
    staDevices3.Add(wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(5)));

    NS_TEST_ASSERT_MSG_EQ(staDevices1.GetN(), 2, "Failed to install Wi-Fi devices on STA 1 and STA 2");
    NS_TEST_ASSERT_MSG_EQ(staDevices2.GetN(), 2, "Failed to install Wi-Fi devices on STA 3 and STA 4");
    NS_TEST_ASSERT_MSG_EQ(staDevices3.GetN(), 2, "Failed to install Wi-Fi devices on STA 5 and STA 6");
}

// Test for mobility model installation
void TestMobilityInstallation()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(3); // 3 Access Points
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(6); // 6 Stations

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

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    Ptr<MobilityModel> apMobility = wifiApNodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> staMobility = wifiStaNodes.Get(0)->GetObject<MobilityModel>();

    NS_TEST_ASSERT_MSG_NOT_NULL(apMobility, "Mobility model not installed on AP nodes");
    NS_TEST_ASSERT_MSG_NOT_NULL(staMobility, "Mobility model not installed on STA nodes");
}

// Test for IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(3); // 3 Access Points
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(6); // 6 Stations

    WifiHelper wifiHelper;
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiMacHelper macHelper;
    Ssid ssid1 = Ssid("AP1-SSID");
    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));

    NetDeviceContainer apDevices1 = wifiHelper.Install(phyHelper, macHelper, wifiApNodes.Get(0));
    NetDeviceContainer staDevices1 = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes.Get(0));
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces1 = ipv4.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = ipv4.Assign(staDevices1);

    NS_TEST_ASSERT_MSG_EQ(apInterfaces1.GetN(), 1, "Failed to assign IP address to AP 1");
    NS_TEST_ASSERT_MSG_EQ(staInterfaces1.GetN(), 1, "Failed to assign IP address to STA 1");
}

// Test for UDP client-server application
void TestUdpApplication()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(6); // 6 Stations

    uint16_t port = 9; // Discard port (just for testing)
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0)); // Server on STA 1
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(Ipv4Address("10.1.1.1"), port); // Client sending to STA 1
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(5)); // Client on STA 6
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server application");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client application");
}

// Test for NetAnim visualization setup
void TestNetAnimSetup()
{
    NodeContainer wifiApNodes;
    wifiApNodes.Create(3); // 3 Access Points

    AnimationInterface anim("wifi_netanim.xml");
    anim.SetConstantPosition(wifiApNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(wifiApNodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(wifiApNodes.Get(2), 20.0, 0.0);

    // Check if the positions are set correctly for visualization
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(wifiApNodes.Get(0)).x, 0.0, "AP 1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(wifiApNodes.Get(1)).x, 10.0, "AP 2 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(wifiApNodes.Get(2)).x, 20.0, "AP 3 position incorrect");
}

int main()
{
    TestNodeCreation();
    TestApDeviceInstallation();
    TestStaDeviceInstallation();
    TestMobilityInstallation();
    TestIpAddressAssignment();
    TestUdpApplication();
    TestNetAnimSetup();

    return 0;
}
