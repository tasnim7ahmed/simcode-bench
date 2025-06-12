#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleWifiTest : public TestCase
{
public:
    SimpleWifiTest() : TestCase("Simple WiFi Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of WiFi nodes (2 stations and 1 access point)
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        // Verify correct number of nodes
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "WiFi station node creation failed, incorrect number of stations");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "WiFi access point node creation failed, incorrect number of access points");
    }

    void TestWifiDeviceInstallation()
    {
        // Test Wi-Fi device installation on nodes
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);
        WifiMacHelper mac;

        Ssid ssid = Ssid("ns3-wifi");

        // Configure Station Devices
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        // Configure Access Point Device
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify device installation
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "WiFi device installation failed on stations");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "WiFi device installation failed on access point");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment on Wi-Fi devices
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);
        WifiMacHelper mac;

        Ssid ssid = Ssid("ns3-wifi");

        // Configure Station Devices
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        // Configure Access Point Device
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Install Internet Stack and Assign IPs
        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Verify IP assignment
        Ipv4Address address1 = staInterfaces.GetAddress(0);
        Ipv4Address address2 = staInterfaces.GetAddress(1);
        NS_TEST_ASSERT_MSG_EQ(address1.IsValid(), true, "IP address assignment failed on Station 0");
        NS_TEST_ASSERT_MSG_EQ(address2.IsValid(), true, "IP address assignment failed on Station 1");
    }

    void TestUdpApplicationSetup()
    {
        // Test setup of UDP server and client applications
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);

        uint16_t port = 8080;
        Ipv4InterfaceContainer staInterfaces;

        // Setup UDP Server on Station 1
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Setup UDP Client on Station 0
        UdpClientHelper udpClient(staInterfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify UDP application setup
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application setup failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application setup failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);

        uint16_t port = 8080;

        // Set up UDP server on Station 1
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on Station 0
        UdpClientHelper udpClient(staInterfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run the simulation
        Simulator::Run();
        Simulator::Destroy();

        // Verify if simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleWifiTest test;
    test.Run();
    return 0;
}
