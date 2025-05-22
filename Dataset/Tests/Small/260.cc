#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class ManetTestSuite : public TestCase
{
public:
    ManetTestSuite() : TestCase("Test MANET Network Setup") {}
    virtual ~ManetTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityConfiguration();
        TestRoutingProtocolInstallation();
        TestDeviceInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
    }

private:
    // Test the creation of MANET nodes
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(5); // Create 5 nodes

        // Ensure the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 5, "Failed to create 5 MANET nodes.");
    }

    // Test the mobility configuration of the nodes
    void TestMobilityConfiguration()
    {
        NodeContainer nodes;
        nodes.Create(5);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(nodes);

        // Verify that the mobility model is correctly set
        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Failed to set mobility model for node 0.");
    }

    // Test the installation of the AODV routing protocol
    void TestRoutingProtocolInstallation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        AodvHelper aodv;
        Ipv4ListRoutingHelper list;
        list.Add(aodv, 100);

        InternetStackHelper internet;
        internet.SetRoutingHelper(list);
        internet.Install(nodes);

        // Check if the routing helper was correctly installed
        Ptr<Ipv4RoutingProtocol> routingProtocol = nodes.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NE(routingProtocol, nullptr, "Failed to install routing protocol on node 0.");
    }

    // Test the installation of WiFi devices on the nodes
    void TestDeviceInstallation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        // Ensure WiFi devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 5, "Failed to install WiFi devices on all nodes.");
    }

    // Test the assignment of IP addresses to the nodes
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to the nodes
        Ipv4Address ipNode1 = interfaces.GetAddress(0);
        Ipv4Address ipNode2 = interfaces.GetAddress(1);
        NS_TEST_ASSERT_MSG_NE(ipNode1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node 0.");
        NS_TEST_ASSERT_MSG_NE(ipNode2, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node 1.");
    }

    // Test the setup of UDP Echo Server and Client applications
    void TestApplicationSetup()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(4)); // Last node as server
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(3));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // First node as client
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on last node.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on first node.");
    }
};

// Instantiate the test suite
static ManetTestSuite manetTestSuite;
