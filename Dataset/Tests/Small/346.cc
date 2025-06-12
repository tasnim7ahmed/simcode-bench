#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiApStaExample
class WifiApStaTest : public TestCase
{
public:
    WifiApStaTest() : TestCase("Test for Wi-Fi AP and STA Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModelSetup();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer wifiNodes;
    NetDeviceContainer staDevices, apDevices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiNodes.Create(2); // Create 2 nodes: 1 AP, 1 STA
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Failed to create two nodes");
    }

    // Test for Wi-Fi device installation (AP and STA)
    void TestWifiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns-3-ssid");

        // Install STA device
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        staDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

        // Install AP device
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

        // Verify devices installation
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 1, "STA device not installed on STA node");
        NS_TEST_ASSERT_MSG_EQ(apDevices.GetN(), 1, "AP device not installed on AP node");
    }

    // Test for mobility model setup
    void TestMobilityModelSetup()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        // Verify mobility model installation
        Ptr<MobilityModel> mobilityModel = wifiNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on node 0");
    }

    // Test for IP address assignment to devices
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(staDevices);
        address.Assign(apDevices);

        // Verify that IP addresses have been assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    // Test for UDP server setup
    void TestUdpServerSetup()
    {
        UdpEchoServerHelper echoServer(9); // Server port 9
        ApplicationContainer serverApp = echoServer.Install(wifiNodes.Get(0)); // Install on AP node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify server application installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Echo Server not installed on AP node");
    }

    // Test for UDP client setup
    void TestUdpClientSetup()
    {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9); // Server address and port 9
        echoClient.SetAttribute("MaxPackets", UintegerValue(1)); // Send 1 packet
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send 1 packet every 1 second
        echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size 1024 bytes
        ApplicationContainer clientApp = echoClient.Install(wifiNodes.Get(1)); // Install on STA node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify client application installation
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Echo Client not installed on STA node");
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
static WifiApStaTest wifiApStaTestInstance;
