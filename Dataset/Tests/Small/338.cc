#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Mobile Wifi UDP Communication
class MobileWifiUdpExampleTest : public TestCase
{
public:
    MobileWifiUdpExampleTest() : TestCase("Test for Mobile Wifi UDP Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestIpAddressAssignment();
        TestMobilitySetup();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiStaNodes, wifiApNode;
    NetDeviceContainer staDevices, apDevices;
    Ipv4InterfaceContainer staInterfaces, apInterfaces;

    void TestNodeCreation()
    {
        wifiStaNodes.Create(2); // Two mobile devices
        wifiApNode.Create(1);   // One access point

        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Failed to create two mobile nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create the access point node");
    }

    void TestWifiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        WifiMacHelper mac;
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
        staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
        apDevices = wifi.Install(phy, mac, wifiApNode);

        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Failed to install Wi-Fi device on mobile nodes");
        NS_TEST_ASSERT_MSG_EQ(apDevices.GetN(), 1, "Failed to install Wi-Fi device on access point");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        staInterfaces = address.Assign(staDevices);
        apInterfaces = address.Assign(apDevices);

        // Verify that IP addresses are assigned to all nodes
        for (uint32_t i = 0; i < staInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to mobile node");
        }
        for (uint32_t i = 0; i < apInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(apInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to access point node");
        }
    }

    void TestMobilitySetup()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(wifiStaNodes); // Install mobility on mobile nodes
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode); // Install constant position on access point

        // Mobility configuration is done, no direct assertion here
        NS_LOG_INFO("Mobility setup completed for nodes.");
    }

    void TestUdpServerSetup()
    {
        UdpServerHelper udpServer(9); // Listening on port 9
        ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on access point node");
    }

    void TestUdpClientSetup()
    {
        UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9); // Connecting to access point
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Send 10 packets
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets

        ApplicationContainer clientApp1 = udpClient.Install(wifiStaNodes.Get(0)); // First mobile device
        ApplicationContainer clientApp2 = udpClient.Install(wifiStaNodes.Get(1)); // Second mobile device
        clientApp1.Start(Seconds(2.0));
        clientApp1.Stop(Seconds(10.0));
        clientApp2.Start(Seconds(2.0));
        clientApp2.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp1.GetN(), 0, "UDP client application not installed on first mobile node");
        NS_TEST_ASSERT_MSG_GT(clientApp2.GetN(), 0, "UDP client application not installed on second mobile node");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static MobileWifiUdpExampleTest mobileWifiUdpExampleTestInstance;
