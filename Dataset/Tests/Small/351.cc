#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiUdpExample
class WifiUdpTest : public TestCase
{
public:
    WifiUdpTest() : TestCase("Test for Wi-Fi UDP Client-Server with Mobility Model") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiSetup();
        TestMobilityModel();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiNodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiNodes.Create(2); // Create 2 nodes (AP and Client)
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Failed to create two nodes");
    }

    // Test for Wi-Fi setup (AP and Client)
    void TestWifiSetup()
    {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        // Configure AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

        // Configure Client
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer clientDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

        // Verify that the devices were installed
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install AP device");
        NS_TEST_ASSERT_MSG_EQ(clientDevice.GetN(), 1, "Failed to install Client device");
    }

    // Test for mobility model setup
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

        // Verify that mobility was set up correctly
        Ptr<MobilityModel> mobilityModel = wifiNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on Node 0");

        mobilityModel = wifiNodes.Get(1)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on Node 1");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that the internet stack is installed
        Ptr<Ipv4> ipv4 = wifiNodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 0");

        ipv4 = wifiNodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 1");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        interfaces = address.Assign(wifiNodes.Get(0)->GetDevice(0));
        interfaces.Add(address.Assign(wifiNodes.Get(1)->GetDevice(0)));

        // Verify that IP addresses were assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    // Test for UDP server setup
    void TestUdpServerSetup()
    {
        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application was installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Server not installed on AP node");
    }

    // Test for UDP client setup
    void TestUdpClientSetup()
    {
        uint16_t port = 8080;
        UdpClientHelper udpClient(interfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(20));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application was installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Client not installed on Client node");
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
static WifiUdpTest wifiUdpTestInstance;
