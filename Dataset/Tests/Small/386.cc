#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Wi-Fi UDP Example
class WifiUdpExampleTest : public TestCase
{
public:
    WifiUdpExampleTest() : TestCase("Test for Wi-Fi UDP Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModelInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerApplicationSetup();
        TestUdpClientApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        // Check if nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create nodes");
    }

    // Test for Wi-Fi device installation
    void TestWifiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-wifi");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        // Install Wi-Fi devices
        devices = wifi.Install(wifiPhy, wifiMac, nodes);

        // Verify Wi-Fi devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install Wi-Fi devices on nodes");
    }

    // Test for mobility model installation
    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(2),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify mobility model is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Failed to install mobility model on node");
        }
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to devices
        interfaces = ipv4.Assign(devices);

        // Verify IP addresses are assigned correctly
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for UDP server application setup
    void TestUdpServerApplicationSetup()
    {
        uint16_t port = 12345;

        // Set up UDP server on Node 1 (Receiver)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify if server application is installed successfully
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on Node 1");
    }

    // Test for UDP client application setup
    void TestUdpClientApplicationSetup()
    {
        uint16_t port = 12345;

        // Set up UDP client on Node 0 (Sender)
        UdpClientHelper udpClient(interfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify if client application is installed successfully
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on Node 0");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify that simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiUdpExampleTest wifiUdpExampleTestInstance;
