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

// Unit Test Suite for Wifi UDP Communication
class WifiUdpExampleTest : public TestCase
{
public:
    WifiUdpExampleTest() : TestCase("Test for Wifi UDP Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
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
        wifiStaNode.Create(1); // Sender (STA)
        wifiApNode.Create(1);  // Receiver (AP)

        NS_TEST_ASSERT_MSG_EQ(wifiStaNode.GetN(), 1, "Failed to create the sender node");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create the receiver node");
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
        staDevices = wifi.Install(phy, mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
        apDevices = wifi.Install(phy, mac, wifiApNode);

        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 1, "Failed to install Wi-Fi device on sender node");
        NS_TEST_ASSERT_MSG_EQ(apDevices.GetN(), 1, "Failed to install Wi-Fi device on receiver node");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        staInterfaces = address.Assign(staDevices);
        apInterfaces = address.Assign(apDevices);

        // Verify that IP addresses are assigned to both nodes
        for (uint32_t i = 0; i < staInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to sender node");
        }
        for (uint32_t i = 0; i < apInterfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(apInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to receiver node");
        }
    }

    void TestUdpServerSetup()
    {
        UdpServerHelper udpServer(9); // Listening on port 9
        ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on receiver node");
    }

    void TestUdpClientSetup()
    {
        UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9); // Connecting to receiver
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Send 10 packets
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets

        ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0)); // Installing client on sender node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on sender node");
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
