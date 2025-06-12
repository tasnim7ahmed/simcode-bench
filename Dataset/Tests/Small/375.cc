#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiUdpExample
class WifiUdpExampleTest : public TestCase
{
public:
    WifiUdpExampleTest() : TestCase("Test for WifiUdpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModelInstallation();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nApNodes = 1;
    uint32_t nStaNodes = 1;
    NodeContainer ap, sta;
    NetDeviceContainer apDevice, staDevice;
    Ipv4InterfaceContainer apIpIface, staIpIface;

    // Test for node creation
    void TestNodeCreation()
    {
        ap.Create(nApNodes);
        sta.Create(nStaNodes);

        // Check that nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(ap.GetN(), nApNodes, "Failed to create AP node");
        NS_TEST_ASSERT_MSG_EQ(sta.GetN(), nStaNodes, "Failed to create STA node");
    }

    // Test for Wi-Fi device installation
    void TestWifiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));

        // Install Wi-Fi devices on AP and STA
        apDevice = wifi.Install(phy, mac, ap);
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
        staDevice = wifi.Install(phy, mac, sta);

        // Verify that Wi-Fi devices are installed
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), nApNodes, "Failed to install AP Wi-Fi devices");
        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), nStaNodes, "Failed to install STA Wi-Fi devices");
    }

    // Test for mobility model installation
    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;

        // Set constant position model for AP
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(ap);

        // Set random walk mobility model for STA
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(sta);

        // Verify mobility model installation
        Ptr<MobilityModel> apMobility = ap.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> staMobility = sta.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(apMobility, nullptr, "Mobility model not installed on AP node");
        NS_TEST_ASSERT_MSG_NE(staMobility, nullptr, "Mobility model not installed on STA node");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4Address;
        ipv4Address.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to Wi-Fi devices
        apIpIface = ipv4Address.Assign(apDevice);
        staIpIface = ipv4Address.Assign(staDevice);

        // Verify that IP addresses are assigned
        Ipv4Address apIp = apIpIface.GetAddress(0);
        Ipv4Address staIp = staIpIface.GetAddress(0);

        NS_TEST_ASSERT_MSG_NE(apIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to AP");
        NS_TEST_ASSERT_MSG_NE(staIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to STA");
    }

    // Test for UDP client-server application setup
    void TestUdpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up UDP client application on STA
        UdpClientHelper udpClient(staIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(ap.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application is installed
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on AP");

        // Set up UDP server application on AP
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(sta.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application is installed
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on STA");
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
static WifiUdpExampleTest wifiUdpExampleTestInstance;
