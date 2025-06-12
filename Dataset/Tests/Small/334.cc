#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WiFi UDP Communication
class WifiUdpExampleTest : public TestCase
{
public:
    WifiUdpExampleTest() : TestCase("Test for WiFi UDP Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModelInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiStaNode, wifiApNode;
    NetDeviceContainer staDevices, apDevices;
    Ipv4InterfaceContainer staInterfaces, apInterfaces;

    void TestNodeCreation()
    {
        wifiStaNode.Create(1); // Station (STA)
        wifiApNode.Create(1);  // Access Point (AP)
        NS_TEST_ASSERT_MSG_EQ(wifiStaNode.GetN(), 1, "Failed to create STA node");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create AP node");
    }

    void TestWifiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        WifiMacHelper mac;
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ns3-ssid")));
        staDevices = wifi.Install(phy, mac, wifiStaNode);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ns3-ssid")));
        apDevices = wifi.Install(phy, mac, wifiApNode);

        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 1, "Failed to install STA WiFi device");
        NS_TEST_ASSERT_MSG_EQ(apDevices.GetN(), 1, "Failed to install AP WiFi device");
    }

    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;

        // Mobility for STA (RandomWalk2dMobilityModel)
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(wifiStaNode);

        // Mobility for AP (ConstantPositionMobilityModel)
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);

        // Verify that mobility models are installed
        Ptr<MobilityModel> mobilityModelSta = wifiStaNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mobilityModelAp = wifiApNode.Get(0)->GetObject<MobilityModel>();
        
        NS_TEST_ASSERT_MSG_NE(mobilityModelSta, nullptr, "Mobility model not installed on STA node");
        NS_TEST_ASSERT_MSG_NE(mobilityModelAp, nullptr, "Mobility model not installed on AP node");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");

        staInterfaces = address.Assign(staDevices);
        apInterfaces = address.Assign(apDevices);

        // Verify that IP addresses are assigned
        for (uint32_t i = 0; i < staInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to STA");
        }
        for (uint32_t i = 0; i < apInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(apInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to AP");
        }
    }

    void TestUdpServerSetup()
    {
        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on AP node");
    }

    void TestUdpClientSetup()
    {
        UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // 10 packets to send
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 second interval
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets

        ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on STA node");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiUdpExampleTest wifiUdpExampleTestInstance;
