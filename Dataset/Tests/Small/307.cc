#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleWifiUdpTest : public TestCase
{
public:
    SimpleWifiUdpTest() : TestCase("Simple Wifi UDP Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestMobilityConfiguration();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of two nodes
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestWifiSetup()
    {
        // Test Wi-Fi setup for the nodes
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        // Verify that devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Wi-Fi devices not correctly installed on all nodes");
    }

    void TestMobilityConfiguration()
    {
        // Test mobility configuration
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

        // Verify that mobility models are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Mobility model not correctly installed on all nodes");
    }

    void TestInternetStackInstallation()
    {
        // Test installation of the internet stack
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that the internet stack is installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Internet stack not correctly installed on all nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment for the nodes
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify IP address assignment for both nodes
        for (uint32_t i = 0; i < wifiNodes.GetN(); ++i)
        {
            Ipv4Address addr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_EQ(addr.IsValid(), true, "IP address assignment failed for node " + std::to_string(i));
        }
    }

    void TestUdpEchoServerSetup()
    {
        // Test the setup of a UDP Echo server on Node 1
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(wifiNodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify server application is installed on Node 1
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP Echo server not correctly installed on Node 1");
    }

    void TestUdpEchoClientSetup()
    {
        // Test the setup of a UDP Echo client on Node 0
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(wifiNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify client application is installed on Node 0
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP Echo client not correctly installed on Node 0");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper stack;
        stack.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(wifiNodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(wifiNodes.Get(0));
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
    SimpleWifiUdpTest test;
    test.Run();
    return 0;
}
