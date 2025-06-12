#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Wi-Fi Example
class WifiExampleTest : public TestCase
{
public:
    WifiExampleTest() : TestCase("Test for Wi-Fi Example") {}
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
    uint32_t nNodes = 2;
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        // Verify nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the expected number of nodes");
    }

    // Test for Wi-Fi device setup and channel configuration
    void TestWifiDeviceSetup()
    {
        // Set up Wi-Fi devices and configure the SSID
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
        WifiMacHelper mac;
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install Wi-Fi devices on nodes");

        // Verify the SSID is set correctly
        Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(devices.Get(0));
        Ptr<WifiMac> mac0 = wifiDevice->GetMac();
        Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac>(mac0);
        NS_TEST_ASSERT_MSG_EQ(staMac->GetSsid(), Ssid("wifi-network"), "Incorrect SSID configured");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that Internet stack is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Failed to install Internet stack on node");
        }
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.1.0", "255.255.255.0");

        // Assign IP addresses to nodes
        interfaces = ipv4.Assign(nodes);

        // Verify that IP addresses are assigned to nodes
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for UDP server setup
    void TestUdpServerSetup()
    {
        uint16_t port = 9;
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that server application was installed successfully
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP server on Node 1");
    }

    // Test for UDP client setup
    void TestUdpClientSetup()
    {
        uint16_t port = 9;
        UdpClientHelper client(interfaces.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that client application was installed successfully
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP client on Node 0");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error during execution");
    }
};

// Register the test
static WifiExampleTest wifiExampleTestInstance;
