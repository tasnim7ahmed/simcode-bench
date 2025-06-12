#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Wi-Fi UDP Communication
class WifiUdpExampleTest : public TestCase
{
public:
    WifiUdpExampleTest() : TestCase("Test for Wi-Fi UDP Communication Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceSetup();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    void TestNodeCreation()
    {
        nodes.Create(2); // Create two nodes: one for the client and one for the server
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    void TestWifiDeviceSetup()
    {
        // Create and configure Wi-Fi devices
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

        WifiHelper wifiAp;
        wifiAp.SetStandard(WIFI_PHY_STANDARD_80211b);
        WifiMacHelper macAp;
        macAp.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid));

        devices.Add(wifi.Install(phy, mac, nodes.Get(0)));
        devices.Add(wifiAp.Install(phy, macAp, nodes.Get(1)));

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install Wi-Fi devices on nodes");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly to both nodes
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestUdpServerSetup()
    {
        uint16_t port = 9; // Port number
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1)); // Install on server node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on server node");
    }

    void TestUdpClientSetup()
    {
        uint16_t port = 9; // Port number
        UdpClientHelper udpClient(interfaces.GetAddress(1), port); // Connect to server node
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));    // Send 100 packets
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 seconds
        udpClient.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes packets
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Install on client node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on client node");
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
