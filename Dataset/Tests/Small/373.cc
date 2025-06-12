#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleWiFiNetworkExample
class SimpleWiFiNetworkTest : public TestCase
{
public:
    SimpleWiFiNetworkTest() : TestCase("Test for SimpleWiFiNetworkExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWiFiDeviceInstallation();
        TestMobilityModelInstallation();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 3;
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the correct number of nodes");
    }

    // Test for WiFi device installation
    void TestWiFiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("simple-wifi");
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        devices = wifi.Install(phy, mac, nodes);

        // Verify that WiFi devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install WiFi devices on all nodes");
    }

    // Test for mobility model installation
    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify that mobility model is installed
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed on node " << i);
        }
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to all devices
        for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
            Ipv4Address ip = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to device " << i);
        }
    }

    // Test for UDP client-server application setup
    void TestUdpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up UDP client application on Node 1 (sending)
        UdpClientHelper udpClient(interfaces.GetAddress(2), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application is installed
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on node 1");

        // Set up UDP server application on Node 2 (receiving)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(2));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application is installed
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on node 2");
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
static SimpleWiFiNetworkTest simpleWiFiNetworkTestInstance;
