#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WiFi Network Example
class WiFiExampleTest : public TestCase
{
public:
    WiFiExampleTest() : TestCase("Test for Wi-Fi Network Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWiFiDeviceInstallation();
        TestMobilityModelInstallation();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpClientServerApplicationSetup();
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

        // Check that nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create Wi-Fi nodes");
    }

    // Test for Wi-Fi device installation
    void TestWiFiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Set up Wi-Fi channel
        YansWifiChannelHelper wifiChannel;
        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");

        YansWifiPhyHelper wifiPhy;
        wifiPhy.SetChannel(wifiChannel.Create());

        // Set up MAC
        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(Ssid("ns-3-ssid")),
                        "Active", BooleanValue(true));

        // Install Wi-Fi devices on nodes
        devices = wifi.Install(wifiPhy, wifiMac, nodes);

        // Verify that devices are installed
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

        // Verify that the mobility model is installed successfully
        MobilityModelHelper model;
        for (uint32_t i = 0; i < nNodes; ++i)
        {
            Ptr<ConstantPositionMobilityModel> mobModel = nodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobModel, "Mobility model not installed correctly on node");
        }
    }

    // Test for internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the internet stack is installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Internet stack installation failed on nodes");
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

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for UDP client-server application setup
    void TestUdpClientServerApplicationSetup()
    {
        uint16_t port = 9; // Port number for the UDP connection

        // Set up UDP server on Node 0 (Access Point)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify UDP server installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on Node 0");

        // Set up UDP client on Node 1 (Station)
        UdpClientHelper udpClient(interfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify UDP client installation
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on Node 1");
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
static WiFiExampleTest wifiExampleTestInstance;
