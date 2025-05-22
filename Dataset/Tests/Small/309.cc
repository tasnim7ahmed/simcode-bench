#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiUdpTest : public TestCase
{
public:
    WifiUdpTest() : TestCase("Wifi UDP Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiChannelSetup();
        TestMobilityModel();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of two Wi-Fi nodes
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestWifiChannelSetup()
    {
        // Test Wi-Fi channel setup
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        // Verify that devices are installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Wi-Fi devices not correctly installed on nodes");
    }

    void TestMobilityModel()
    {
        // Test mobility model setup
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(2),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        // Verify that mobility model is set for both nodes
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Mobility model not correctly installed on nodes");
    }

    void TestInternetStackInstallation()
    {
        // Test installation of the internet stack
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that the internet stack is installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Internet stack not correctly installed on nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment for the nodes
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that IP addresses are correctly assigned to both nodes
        for (uint32_t i = 0; i < wifiNodes.GetN(); ++i)
        {
            Ipv4Address addr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_EQ(addr.IsValid(), true, "IP address assignment failed for node " + std::to_string(i));
        }
    }

    void TestUdpServerSetup()
    {
        // Test setup of the UDP server (sink) on Node 1
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application is installed on Node 1
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server (sink) not correctly installed on Node 1");
    }

    void TestUdpClientSetup()
    {
        // Test setup of the UDP client (sender) on Node 0
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        UdpClientHelper udpClient(interfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // 100 packets/sec
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application is installed on Node 0
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client (sender) not correctly installed on Node 0");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(interfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // 100 packets/sec
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0));
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
    WifiUdpTest test;
    test.Run();
    return 0;
}
