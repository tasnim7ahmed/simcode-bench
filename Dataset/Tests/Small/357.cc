#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleWiFiExample
class SimpleWiFiTest : public TestCase
{
public:
    SimpleWiFiTest() : TestCase("Test for Simple WiFi Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWiFiSetup();
        TestMobilityModelSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpApplicationsSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiStaNode;
    NodeContainer wifiApNode;
    NodeContainer wifiNodes;
    NetDeviceContainer staDevice, apDevice;
    Ipv4InterfaceContainer staInterface, apInterface;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiStaNode.Create(1);
        wifiApNode.Create(1);
        wifiNodes = NodeContainer(wifiApNode, wifiStaNode);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Failed to create the correct number of nodes");
    }

    // Test for Wi-Fi setup
    void TestWiFiSetup()
    {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-wifi");

        // AP setup
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode);

        // STA setup
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        staDevice = wifi.Install(wifiPhy, wifiMac, wifiStaNode);

        // Verify Wi-Fi setup
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install AP device");
        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "Failed to install STA device");
    }

    // Test for mobility model setup
    void TestMobilityModelSetup()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        // Verify mobility model setup
        Ptr<MobilityModel> mobilityModel = wifiStaNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on STA");

        mobilityModel = wifiApNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on AP");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that the Internet stack is installed
        Ptr<Ipv4> ipv4 = wifiStaNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on STA");

        ipv4 = wifiApNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on AP");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        staInterface = address.Assign(staDevice);
        apInterface = address.Assign(apDevice);

        // Verify that the IP addresses were assigned correctly
        NS_TEST_ASSERT_MSG_NE(staInterface.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to STA");
        NS_TEST_ASSERT_MSG_NE(apInterface.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to AP");
    }

    // Test for UDP Echo server and client applications setup
    void TestUdpApplicationsSetup()
    {
        uint16_t port = 9;

        // Set up UDP Echo Server on AP
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP Echo Client on STA
        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify application setup
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Echo server application not installed on AP");
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Echo client application not installed on STA");
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
static SimpleWiFiTest simpleWiFiTestInstance;
