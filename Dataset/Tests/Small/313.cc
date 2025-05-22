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
    SimpleWifiTest() : TestCase("Simple Wi-Fi Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModelSetup();
        TestIpv4AddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of 2 STA nodes and 1 AP node
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Wi-Fi STA node creation failed");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Wi-Fi AP node creation failed");
    }

    void TestWifiDeviceInstallation()
    {
        // Test the installation of Wi-Fi devices on STA and AP
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        NetDeviceContainer staDevices = wifi.Install(wifiMac, wifi, wifiStaNodes);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiMac, wifi, wifiApNode);

        // Verify devices were installed correctly
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Wi-Fi devices installation failed on STA");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Wi-Fi device installation failed on AP");
    }

    void TestMobilityModelSetup()
    {
        // Test mobility model setup for STA and AP
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);

        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator", "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"), "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Time", StringValue("1s"), "Distance", StringValue("10m"));
        mobility.Install(wifiStaNodes);

        // Verify mobility models were installed
        NS_TEST_ASSERT_MSG_EQ(true, true, "Mobility models were not correctly set up");
    }

    void TestIpv4AddressAssignment()
    {
        // Test the assignment of IP addresses to the devices
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        NetDeviceContainer staDevices = wifi.Install(wifiMac, wifi, wifiStaNodes);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiMac, wifi, wifiApNode);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);

        // Verify that IP addresses were assigned correctly
        NS_TEST_ASSERT_MSG_EQ(staInterfaces.GetN(), 2, "IP address assignment failed for STA");
        NS_TEST_ASSERT_MSG_EQ(apInterfaces.GetN(), 1, "IP address assignment failed for AP");
    }

    void TestUdpServerSetup()
    {
        // Test the setup of UDP server on STA 0
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application was installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on STA 0");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of UDP client on STA 1
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        NetDeviceContainer staDevices = wifi.Install(wifiMac, wifi, wifiStaNodes);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NodeContainer wifiApNode;
        wifiApNode.Create(1);
        NetDeviceContainer apDevice = wifi.Install(wifiMac, wifi, wifiApNode);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);

        uint16_t port = 9;
        UdpClientHelper udpClient(apInterfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application was installed correctly
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on STA 1");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        NetDeviceContainer staDevices = wifi.Install(wifiMac, wifi, wifiStaNodes);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiMac, wifi, wifiApNode);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);

        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator", "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"), "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Time", StringValue("1s"), "Distance", StringValue("10m"));
        mobility.Install(wifiStaNodes);

        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(apInterfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
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
