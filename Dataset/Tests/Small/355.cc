#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleWiFiExample
class SimpleWiFiTest : public TestCase
{
public:
    SimpleWiFiTest() : TestCase("Test for Simple Wi-Fi Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWiFiSetup();
        TestMobilityModel();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiNodes;
    Ipv4InterfaceContainer wifiInterfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiNodes.Create(3);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 3, "Failed to create the correct number of Wi-Fi nodes");
    }

    // Test for Wi-Fi setup (PHY, MAC layers, and devices)
    void TestWiFiSetup()
    {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper wifiMac;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        Ssid ssid = Ssid("ns3-wifi");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

        // Verify that devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(wifiDevices.GetN(), 3, "Wi-Fi devices not installed correctly on all nodes");
    }

    // Test for mobility model installation
    void TestMobilityModel()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        // Verify mobility model installation
        Ptr<ConstantPositionMobilityModel> mobilityModel = wifiNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed correctly on node 0");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper internet;
        internet.Install(wifiNodes);

        // Verify that the Internet stack is installed
        Ptr<Ipv4> ipv4 = wifiNodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = wifiNodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 1");

        ipv4 = wifiNodes.Get(2)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 2");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        wifiInterfaces = address.Assign(wifiNodes.Get(0)->GetDevice(0));
        address.Assign(wifiNodes.Get(1)->GetDevice(0));
        address.Assign(wifiNodes.Get(2)->GetDevice(0));

        // Verify that the IP addresses were assigned correctly
        for (uint32_t i = 0; i < wifiInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(wifiInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned");
        }
    }

    // Test for UDP Echo Server setup
    void TestUdpEchoServerSetup()
    {
        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(wifiNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Echo Server not installed on node 0");
    }

    // Test for UDP Echo Client setup
    void TestUdpEchoClientSetup()
    {
        uint16_t port = 9;
        UdpEchoClientHelper echoClient(wifiInterfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(wifiNodes.Get(2));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Echo Client not installed on node 2");
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
