#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiUdpExample
class WifiUdpTest : public TestCase
{
public:
    WifiUdpTest() : TestCase("Test for WifiUdpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiStackInstallation();
        TestDeviceInstallation();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nStaNodes = 1;
    uint32_t nApNodes = 1;
    NodeContainer wifiStaNode;
    NodeContainer wifiApNode;
    NetDeviceContainer staDevice;
    NetDeviceContainer apDevice;
    Ipv4InterfaceContainer staInterface;
    WifiHelper wifi;
    InternetStackHelper internet;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiStaNode.Create(nStaNodes);
        wifiApNode.Create(nApNodes);

        NS_TEST_ASSERT_MSG_EQ(wifiStaNode.GetN(), nStaNodes, "Failed to create the correct number of STA nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), nApNodes, "Failed to create the correct number of AP nodes");
    }

    // Test for Wi-Fi stack installation
    void TestWifiStackInstallation()
    {
        internet.Install(wifiStaNode);
        internet.Install(wifiApNode);

        // Verify that the Internet stack is installed on both nodes
        NS_TEST_ASSERT_MSG_NE(wifiStaNode.Get(0)->GetObject<Ipv4>(), nullptr, "IPv4 stack not installed on STA node");
        NS_TEST_ASSERT_MSG_NE(wifiApNode.Get(0)->GetObject<Ipv4>(), nullptr, "IPv4 stack not installed on AP node");
    }

    // Test for device installation (STA and AP)
    void TestDeviceInstallation()
    {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns-3-ssid");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevice = wifi.Install(phy, mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify that devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "Failed to install STA device");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install AP device");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        staInterface = ipv4.Assign(staDevice);

        // Verify that IP addresses are assigned correctly to the STA
        NS_TEST_ASSERT_MSG_NE(staInterface.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IPv4 address assigned to STA node");
    }

    // Test for UDP application setup (client and server)
    void TestUdpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up UDP client application on STA (client)
        UdpClientHelper udpClient(staInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application is installed on AP
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on AP node");

        // Set up UDP server application on AP (server)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiStaNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application is installed on STA
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on STA node");
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
static WifiUdpTest wifiUdpTestInstance;
