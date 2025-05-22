#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleWiFiExample
class SimpleWiFiTest : public TestCase
{
public:
    SimpleWiFiTest() : TestCase("Test for Simple WiFi Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpApplicationsSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nWifiNodes = 2;
    NodeContainer wifiNodes;
    NetDeviceContainer wifiDevices;
    Ipv4InterfaceContainer wifiInterfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiNodes.Create(nWifiNodes);
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), nWifiNodes, "Failed to create the correct number of Wi-Fi nodes");
    }

    // Test for Wi-Fi device setup
    void TestWifiDeviceSetup()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-default")));

        wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

        // Verify Wi-Fi devices are installed
        NS_TEST_ASSERT_MSG_EQ(wifiDevices.GetN(), nWifiNodes, "Failed to install Wi-Fi devices on all nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that the Internet stack is installed on the nodes
        Ptr<Ipv4> ipv4 = wifiNodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = wifiNodes.Get(nWifiNodes - 1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on last node");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        wifiInterfaces = address.Assign(wifiDevices);

        // Verify that the IP addresses were assigned correctly
        for (uint32_t i = 0; i < nWifiNodes; ++i)
        {
            NS_TEST_ASSERT_MSG_NE(wifiInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node " << i);
        }
    }

    // Test for TCP Echo server and client applications setup
    void TestTcpApplicationsSetup()
    {
        uint16_t port = 8080;

        // Set up TCP Echo Server on the second node (server)
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(wifiNodes.Get(nWifiNodes - 1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up TCP Echo Client on the first node (client)
        TcpClientHelper tcpClient(wifiInterfaces.GetAddress(nWifiNodes - 1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(wifiNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify application setup
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP Echo server application not installed on server node");
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP Echo client application not installed on client node");
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
static SimpleWiFiTest simpleWiFiTestInstance;
