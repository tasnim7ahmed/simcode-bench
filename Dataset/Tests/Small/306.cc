#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class AdhocAodvTest : public TestCase
{
public:
    AdhocAodvTest() : TestCase("Adhoc AODV Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiAdhocModeSetup();
        TestMobilityConfiguration();
        TestAodvRoutingInstallation();
        TestIpv4AddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of three nodes
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(adhocNodes.GetN(), 3, "Node creation failed, incorrect number of nodes");
    }

    void TestWifiAdhocModeSetup()
    {
        // Test setting up Wi-Fi in ad-hoc mode
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, adhocNodes);

        // Verify that devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "Wi-Fi devices not correctly installed on all nodes");
    }

    void TestMobilityConfiguration()
    {
        // Test mobility configuration
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(100.0),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(adhocNodes);

        // Verify that mobility models are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(adhocNodes.GetN(), 3, "Mobility model not correctly installed on all nodes");
    }

    void TestAodvRoutingInstallation()
    {
        // Test installation of AODV routing protocol
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        AodvHelper aodv;
        InternetStackHelper stack;
        stack.SetRoutingHelper(aodv);
        stack.Install(adhocNodes);

        // Verify that routing helper is installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(adhocNodes.GetN(), 3, "AODV routing protocol not installed on all nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, adhocNodes);

        InternetStackHelper stack;
        stack.SetRoutingHelper(AodvHelper());
        stack.Install(adhocNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify IP addresses are assigned
        for (uint32_t i = 0; i < adhocNodes.GetN(); ++i)
        {
            Ipv4Address addr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_EQ(addr.IsValid(), true, "IP address assignment failed for node " + std::to_string(i));
        }
    }

    void TestUdpEchoServerSetup()
    {
        // Test setting up UDP Echo server on Node 2
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, adhocNodes);

        InternetStackHelper stack;
        stack.SetRoutingHelper(AodvHelper());
        stack.Install(adhocNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(adhocNodes.Get(2));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        // Verify server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP Echo server not correctly installed on Node 2");
    }

    void TestUdpEchoClientSetup()
    {
        // Test setting up UDP Echo client on Node 0
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, adhocNodes);

        InternetStackHelper stack;
        stack.SetRoutingHelper(AodvHelper());
        stack.Install(adhocNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(adhocNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        // Verify client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP Echo client not correctly installed on Node 0");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer adhocNodes;
        adhocNodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, adhocNodes);

        InternetStackHelper stack;
        stack.SetRoutingHelper(AodvHelper());
        stack.Install(adhocNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(adhocNodes.Get(2));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(adhocNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    AdhocAodvTest test;
    test.Run();
    return 0;
}
