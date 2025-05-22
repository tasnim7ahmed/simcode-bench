#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiSimpleExample
class WifiSimpleTest : public TestCase
{
public:
    WifiSimpleTest() : TestCase("Test for Wifi Simple Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiSetup();
        TestMobilitySetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiStaNode, wifiApNode;
    Ipv4InterfaceContainer staInterface, apInterface;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiStaNode.Create(1);
        wifiApNode.Create(1);
        
        NS_TEST_ASSERT_MSG_EQ(wifiStaNode.GetN(), 1, "Failed to create station node");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create access point node");
    }

    // Test for Wi-Fi setup
    void TestWifiSetup()
    {
        // Set up the Wi-Fi channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-ssid");

        // Configure station device
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

        // Configure AP device
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify that devices were installed
        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "Station device not installed correctly");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Access Point device not installed correctly");
    }

    // Test for mobility setup
    void TestMobilitySetup()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(2),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNode);
        mobility.Install(wifiApNode);

        // Verify that mobility model was applied
        Ptr<MobilityModel> mobilityModel = wifiStaNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on station node");

        mobilityModel = wifiApNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on AP node");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiStaNode);
        stack.Install(wifiApNode);

        // Verify that the Internet stack is installed
        Ptr<Ipv4> ipv4 = wifiStaNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on station node");

        ipv4 = wifiApNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on AP node");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        staInterface = address.Assign(wifiStaNode.Get(0)->GetDevice(0));
        apInterface = address.Assign(wifiApNode.Get(0)->GetDevice(0));

        // Verify that IP addresses were assigned correctly
        for (uint32_t i = 0; i < staInterface.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(staInterface.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to station");
        }

        for (uint32_t i = 0; i < apInterface.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(apInterface.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to access point");
        }
    }

    // Test for UDP server setup
    void TestUdpServerSetup()
    {
        uint16_t port = 4000;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server was installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Server not installed on AP node");
    }

    // Test for UDP client setup
    void TestUdpClientSetup()
    {
        uint16_t port = 4000;
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client was installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Client not installed on station node");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiSimpleTest wifiSimpleTestInstance;
