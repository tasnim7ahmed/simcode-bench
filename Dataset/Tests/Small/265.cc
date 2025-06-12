#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiNetworkTestSuite : public TestCase
{
public:
    WifiNetworkTestSuite() : TestCase("Test Wi-Fi Network Setup for AP and Stations") {}
    virtual ~WifiNetworkTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify AP and stations are created successfully)
    void TestNodeCreation()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2); // Create 2 stations
        NodeContainer wifiApNode;
        wifiApNode.Create(1); // Create 1 access point

        // Verify that the nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Failed to create the expected number of station nodes.");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create the expected number of access point nodes.");
    }

    // Test Wi-Fi device installation (verify devices are installed correctly)
    void TestWifiDeviceInstallation()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify that devices are installed correctly on both AP and stations
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Failed to install Wi-Fi devices on station nodes.");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on AP node.");
    }

    // Test IP address assignment (verify that IP addresses are assigned correctly to all devices)
    void TestIpAddressAssignment()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Verify that the IP addresses are assigned correctly
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = staInterfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to station.");
        }
        Ipv4Address apIpAddr = apInterface.GetAddress(0);
        NS_TEST_ASSERT_MSG_NE(apIpAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to access point.");
    }

    // Test UDP Echo Server setup (verify that the server application is installed correctly)
    void TestUdpEchoServerSetup()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0)); // Install on AP node
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on the AP.");
    }

    // Test UDP Echo Client setup (verify that the client application is installed correctly)
    void TestUdpEchoClientSetup()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0)); // Install on first station
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on the station.");
    }

    // Test data transmission (verify that data is transmitted between client and server)
    void TestDataTransmission()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0)); // Install on AP node
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0)); // Install on first station
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Additional check to verify that client-server communication was successful could be added here.
    }
};

// Register the test suite
TestSuite wifiTestSuite("WifiNetworkTestSuite", SYSTEM);
wifiTestSuite.AddTestCase(new WifiNetworkTestSuite());
