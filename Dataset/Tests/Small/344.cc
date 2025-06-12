#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Wifi TCP Multiple Clients
class WifiTcpMultipleClientsTest : public TestCase
{
public:
    WifiTcpMultipleClientsTest() : TestCase("Test for WiFi TCP Multiple Clients Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiLinkConfiguration();
        TestDeviceInstallation();
        TestIpAddressAssignment();
        TestMobilitySetup();
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
        nodes.Create(3); // Create 3 nodes: 2 clients and 1 server
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Failed to create three nodes");
    }

    void TestWifiLinkConfiguration()
    {
        // Setup WiFi channel and PHY layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Setup MAC layer and install devices
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");
        WifiMacHelper mac;
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
        devices = wifi.Install(phy, mac, nodes);

        // Verify devices installation
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "Failed to install WiFi devices on all nodes");
    }

    void TestDeviceInstallation()
    {
        // Verify devices are installed correctly
        for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
            Ptr<NetDevice> device = devices.Get(i);
            NS_TEST_ASSERT_MSG_NOT_NULL(device, "Device not installed on node");
        }
    }

    void TestIpAddressAssignment()
    {
        // Assign IP addresses to devices
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify IP assignment
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestMobilitySetup()
    {
        // Set up mobility for nodes (static positions)
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

        // Verify mobility is set up correctly
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> model = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(model, "Mobility model not installed on node");
        }
    }

    void TestTcpServerSetup()
    {
        uint16_t port = 9; // Port for server
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(2)); // Install on server node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        // Verify server application is installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP Server application not installed on server node");
    }

    void TestTcpClientSetup()
    {
        uint16_t port = 9; // Port for client
        TcpClientHelper tcpClient1(interfaces.GetAddress(2), port); // Server address and port 9
        tcpClient1.SetAttribute("MaxPackets", UintegerValue(10));   // Send 10 packets
        tcpClient1.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 second
        tcpClient1.SetAttribute("PacketSize", UintegerValue(512));   // 512 bytes packet
        ApplicationContainer clientApp1 = tcpClient1.Install(nodes.Get(0)); // Install on client node 0
        clientApp1.Start(Seconds(2.0));
        clientApp1.Stop(Seconds(20.0));

        TcpClientHelper tcpClient2(interfaces.GetAddress(2), port); // Server address and port 9
        tcpClient2.SetAttribute("MaxPackets", UintegerValue(10));   // Send 10 packets
        tcpClient2.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 second
        tcpClient2.SetAttribute("PacketSize", UintegerValue(512));   // 512 bytes packet
        ApplicationContainer clientApp2 = tcpClient2.Install(nodes.Get(1)); // Install on client node 1
        clientApp2.Start(Seconds(3.0));
        clientApp2.Stop(Seconds(20.0));

        // Verify client applications are installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp1.GetN(), 0, "TCP Client application not installed on client node 0");
        NS_TEST_ASSERT_MSG_GT(clientApp2.GetN(), 0, "TCP Client application not installed on client node 1");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiTcpMultipleClientsTest wifiTcpMultipleClientsTestInstance;
