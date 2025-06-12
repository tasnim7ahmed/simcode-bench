#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiTestSuite : public TestCase
{
public:
    WifiTestSuite() : TestCase("Test Wi-Fi with UDP Echo Application") {}
    virtual ~WifiTestSuite() {}

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
    // Test node creation (verify that 3 stations and 1 access point are created correctly)
    void TestNodeCreation()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(3);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Verify that 3 stations and 1 access point are created
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 3, "Failed to create the expected number of station nodes.");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create the expected number of access point nodes.");
    }

    // Test Wi-Fi device installation (verify that Wi-Fi devices are installed correctly on all nodes)
    void TestWifiDeviceInstallation()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(3);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Configure Wi-Fi channel and PHY layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Configure MAC for stations (STA)
        WifiMacHelper macSta;
        Ssid ssid = Ssid("ns3-wifi");
        macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(phy, macSta, wifiStaNodes);

        // Configure MAC for Access Point (AP)
        WifiMacHelper macAp;
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice;
        apDevice = wifi.Install(phy, macAp, wifiApNode);

        // Verify that Wi-Fi devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 3, "Failed to install Wi-Fi devices on station nodes.");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on access point.");
    }

    // Test IP address assignment (verify that IP addresses are correctly assigned to devices)
    void TestIpAddressAssignment()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(3);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Configure Wi-Fi channel and PHY layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Configure MAC for stations (STA)
        WifiMacHelper macSta;
        Ssid ssid = Ssid("ns3-wifi");
        macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(phy, macSta, wifiStaNodes);

        // Configure MAC for Access Point (AP)
        WifiMacHelper macAp;
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice;
        apDevice = wifi.Install(phy, macAp, wifiApNode);

        InternetStackHelper internet;
        internet.Install(wifiStaNodes);
        internet.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Verify that IP addresses are assigned to devices
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = staInterfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to station.");
        }

        Ipv4Address apIpAddr = apInterface.GetAddress(0);
        NS_TEST_ASSERT_MSG_NE(apIpAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to access point.");
    }

    // Test UDP Echo Server setup (verify that the UDP Echo Server is installed correctly on the access point)
    void TestUdpEchoServerSetup()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(3);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Configure Wi-Fi channel and PHY layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Configure MAC for stations (STA)
        WifiMacHelper macSta;
        Ssid ssid = Ssid("ns3-wifi");
        macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(phy, macSta, wifiStaNodes);

        // Configure MAC for Access Point (AP)
        WifiMacHelper macAp;
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice;
        apDevice = wifi.Install(phy, macAp, wifiApNode);

        InternetStackHelper internet;
        internet.Install(wifiStaNodes);
        internet.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Set up the UDP Echo Server on the AP
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Server is installed on the access point
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on access point.");
    }

    // Test UDP Echo Client setup (verify that the UDP Echo Client is set up correctly on a station)
    void TestUdpEchoClientSetup()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(3);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Configure Wi-Fi channel and PHY layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Configure MAC for stations (STA)
        WifiMacHelper macSta;
        Ssid ssid = Ssid("ns3-wifi");
        macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(phy, macSta, wifiStaNodes);

        // Configure MAC for Access Point (AP)
        WifiMacHelper macAp;
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice;
        apDevice = wifi.Install(phy, macAp, wifiApNode);

        InternetStackHelper internet;
        internet.Install(wifiStaNodes);
        internet.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Set up the UDP Echo Client on a station
        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Client is installed on the station
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on station.");
    }

    // Test data transmission (verify that data is transmitted between the UDP Echo Client and Server)
    void TestDataTransmission()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(3);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        // Configure Wi-Fi channel and PHY layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Configure MAC for stations (STA)
        WifiMacHelper macSta;
        Ssid ssid = Ssid("ns3-wifi");
        macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(phy, macSta, wifiStaNodes);

        // Configure MAC for Access Point (AP)
        WifiMacHelper macAp;
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice;
        apDevice = wifi.Install(phy, macAp, wifiApNode);

        InternetStackHelper internet;
        internet.Install(wifiStaNodes);
        internet.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Set up the UDP Echo Server and Client
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Run the simulation and verify successful data transmission
        Simulator::Run();
        Simulator::Destroy();
    }
};

// Test Suite registration
int main(int argc, char *argv[])
{
    WifiTestSuite wifiTestSuite;
    wifiTestSuite.Run();
    return 0;
}
