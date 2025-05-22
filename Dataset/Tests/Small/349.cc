#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiUdpClientServerExample
class WifiUdpClientServerTest : public TestCase
{
public:
    WifiUdpClientServerTest() : TestCase("Test for Wi-Fi UDP Client-Server Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(2); // Create 2 nodes (server and client)
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    // Test for Wi-Fi device setup (PHY, MAC, and channel)
    void TestWifiDeviceSetup()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-adhoc-network");
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        // Verify that devices were installed correctly on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install Wi-Fi devices on nodes");
    }

    // Test for internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the internet stack was installed on both nodes
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 0");
        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 1");
    }

    // Test for IP address assignment to nodes
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(nodes.Get(0)->GetDevice(0), nodes.Get(1)->GetDevice(0));

        // Verify that IP addresses have been assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    // Test for UDP server setup
    void TestUdpServerSetup()
    {
        uint16_t port = 9; // Port number for UDP connection
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application was installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Server not installed on Node 0");
    }

    // Test for UDP client setup
    void TestUdpClientSetup()
    {
        uint16_t port = 9; // Port number for UDP connection
        UdpClientHelper udpClient(interfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Send 10 packets
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // Packet size
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application was installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Client not installed on Node 1");
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
static WifiUdpClientServerTest wifiUdpClientServerTestInstance;
