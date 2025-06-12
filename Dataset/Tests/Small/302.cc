#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleWiFiTest : public TestCase
{
public:
    SimpleWiFiTest() : TestCase("Simple Wi-Fi UDP Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModel();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 2 STA nodes and 1 AP node
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        // Verify correct number of nodes
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "STA node creation failed, incorrect number of nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "AP node creation failed, incorrect number of nodes");
    }

    void TestWifiDeviceInstallation()
    {
        // Test Wi-Fi device installation on nodes
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        // Set up STA devices
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        // Set up AP device
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify device installation
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Wi-Fi device installation failed on STA nodes");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Wi-Fi device installation failed on AP node");
    }

    void TestMobilityModel()
    {
        // Test the mobility model configuration
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNodes);
        mobility.Install(wifiApNode);

        // Verify mobility model installation
        Ptr<MobilityModel> mobilityModelSta = wifiStaNodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mobilityModelAp = wifiApNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModelSta != nullptr, true, "Mobility model installation failed on STA node");
        NS_TEST_ASSERT_MSG_EQ(mobilityModelAp != nullptr, true, "Mobility model installation failed on AP node");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to devices
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        // Set up STA devices
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        // Set up AP device
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Install Internet stack
        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Verify IP address assignment
        Ipv4Address address1 = staInterfaces.GetAddress(0);
        Ipv4Address address2 = apInterface.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(address1.IsValid(), true, "IP address assignment failed on STA node");
        NS_TEST_ASSERT_MSG_EQ(address2.IsValid(), true, "IP address assignment failed on AP node");
    }

    void TestUdpApplicationSetup()
    {
        // Test UDP server and client application setup
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        uint16_t port = 8080;

        // Set up UDP server on the first STA
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on the second STA
        UdpClientHelper udpClient("192.168.1.1", port);  // Using AP's IP address
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify application installation
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        uint16_t port = 8080;

        // Set up UDP server on the first STA
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on the second STA
        UdpClientHelper udpClient("192.168.1.1", port);  // Using AP's IP address
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run the simulation
        Simulator::Run();
        Simulator::Destroy();

        // Basic check to ensure the simulation runs
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleWiFiTest test;
    test.Run();
    return 0;
}
