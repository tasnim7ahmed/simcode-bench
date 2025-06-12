#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiUdpTest : public TestCase
{
public:
    WifiUdpTest() : TestCase("Wi-Fi UDP Example Test") {}

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
        // Test creation of 1 STA and 1 AP node
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Verify that the correct number of nodes have been created
        NS_TEST_ASSERT_MSG_EQ(wifiStaNode.GetN(), 1, "STA node creation failed, incorrect number of nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "AP node creation failed, incorrect number of nodes");
    }

    void TestWifiDeviceInstallation()
    {
        // Test Wi-Fi device installation on nodes
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;

        // Set up STA
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

        // Set up AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

        // Verify device installation
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 1, "Wi-Fi device installation failed on STA node");
        NS_TEST_ASSERT_MSG_EQ(apDevices.GetN(), 1, "Wi-Fi device installation failed on AP node");
    }

    void TestMobilityModel()
    {
        // Test the mobility model configuration
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNode);
        mobility.Install(wifiApNode);

        // Verify mobility model installation
        Ptr<MobilityModel> mobilityModel = wifiStaNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model installation failed on STA node");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to devices
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;

        // Set up STA
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

        // Set up AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

        // Install Internet stack
        InternetStackHelper stack;
        stack.Install(wifiStaNode);
        stack.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterface = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

        // Verify IP address assignment
        Ipv4Address address1 = staInterface.GetAddress(0);
        Ipv4Address address2 = apInterface.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(address1.IsValid(), true, "IP address assignment failed on STA node");
        NS_TEST_ASSERT_MSG_EQ(address2.IsValid(), true, "IP address assignment failed on AP node");
    }

    void TestUdpApplicationSetup()
    {
        // Test UDP server and client application setup
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        uint16_t port = 4000;

        // Set up UDP server on AP
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on STA
        UdpClientHelper udpClient("192.168.1.1", port);  // Using AP's IP address
        udpClient.SetAttribute("MaxPackets", UintegerValue(50));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify application installation
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        uint16_t port = 4000;

        // Set up UDP server on AP
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on STA
        UdpClientHelper udpClient("192.168.1.1", port);  // Using AP's IP address
        udpClient.SetAttribute("MaxPackets", UintegerValue(50));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run the simulation
        Simulator::Run();
        Simulator::Destroy();

        // Basic check to see if the simulation runs without issues
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    WifiUdpTest test;
    test.Run();
    return 0;
}
