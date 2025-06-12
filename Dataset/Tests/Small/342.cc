#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WiFi TCP Communication
class WifiTcpExampleTest : public TestCase
{
public:
    WifiTcpExampleTest() : TestCase("Test for WiFi TCP Communication Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiConfiguration();
        TestMobilitySetup();
        TestDeviceInstallation();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
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

    void TestWifiConfiguration()
    {
        // Set up the WiFi channel (802.11b standard)
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Set up the WiFi mac layer
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")));

        // Install the WiFi devices (NICs)
        devices = wifi.Install(phy, mac, nodes);

        // Verify that the WiFi devices have been installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install WiFi devices on nodes");
    }

    void TestMobilitySetup()
    {
        // Set up mobility for the nodes (Static positions)
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify mobility model is set for both nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on node");
        }
    }

    void TestDeviceInstallation()
    {
        // Verify WiFi devices were installed correctly on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install WiFi devices on nodes");
    }

    void TestIpAddressAssignment()
    {
        // Assign IP addresses to the WiFi devices
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly to both nodes
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestTcpServerSetup()
    {
        uint16_t port = 9; // Port number for server
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Install on server node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        // Verify TCP server application is installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on server node");
    }

    void TestTcpClientSetup()
    {
        uint16_t port = 9; // Port number for client
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port); // Connect to server node
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));    // Send 100 packets
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 seconds
        tcpClient.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes packets
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Install on client node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        // Verify TCP client application is installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on client node");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiTcpExampleTest wifiTcpExampleTestInstance;
