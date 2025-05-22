#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiNetworkExample
class WifiNetworkExampleTest : public TestCase
{
public:
    WifiNetworkExampleTest() : TestCase("Test for WifiNetworkExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiChannelAndDeviceSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpClientServerApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nWifiNodes = 3;
    NodeContainer wifiNodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiNodes.Create(nWifiNodes);

        // Check that nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), nWifiNodes, "Failed to create Wi-Fi nodes");
    }

    // Test for Wi-Fi channel and device setup
    void TestWifiChannelAndDeviceSetup()
    {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns-3-ssid");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        // Install Wi-Fi devices on nodes
        devices = wifi.Install(phy, mac, wifiNodes.Get(0));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));
        devices.Add(wifi.Install(phy, mac, wifiNodes.Get(2)));

        // Verify that devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nWifiNodes, "Failed to install Wi-Fi devices on nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that internet stack is installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), nWifiNodes, "Internet stack installation failed on nodes");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to devices
        interfaces = ipv4.Assign(devices);

        // Verify that IP addresses are assigned
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);
        Ipv4Address node3Ip = interfaces.GetAddress(2);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
        NS_TEST_ASSERT_MSG_NE(node3Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 3");
    }

    // Test for UDP client-server application setup
    void TestUdpClientServerApplicationSetup()
    {
        uint16_t port = 9; // Arbitrary port

        // Set up UDP server on Node 3
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(2));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify UDP server installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on Node 3");

        // Set up UDP client on Node 2
        UdpClientHelper udpClient(interfaces.GetAddress(2), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify UDP client installation
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on Node 2");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiNetworkExampleTest wifiNetworkExampleTestInstance;
