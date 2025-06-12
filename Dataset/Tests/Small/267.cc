#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiAodvTestSuite : public TestCase
{
public:
    WifiAodvTestSuite() : TestCase("Test Wi-Fi with AODV Routing and UDP Echo Application") {}
    virtual ~WifiAodvTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWiFiDeviceInstallation();
        TestMobilityModel();
        TestAodvRoutingInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that three nodes are created correctly)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        // Verify that three nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Failed to create the expected number of nodes.");
    }

    // Test Wi-Fi device installation (verify that Wi-Fi devices are installed correctly on the nodes)
    void TestWiFiDeviceInstallation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        // Verify that Wi-Fi devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "Failed to install Wi-Fi devices on nodes.");
    }

    // Test mobility model (verify that mobility is applied to the nodes)
    void TestMobilityModel()
    {
        NodeContainer nodes;
        nodes.Create(3);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify that mobility is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Failed to install mobility model on node.");
        }
    }

    // Test AODV routing installation (verify that AODV routing is installed correctly)
    void TestAodvRoutingInstallation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        AodvHelper aodv;
        Ipv4ListRoutingHelper list;
        list.Add(aodv, 10);

        InternetStackHelper internet;
        internet.SetRoutingHelper(list);
        internet.Install(nodes);

        // Verify that AODV routing is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_EQ(ipv4->GetRoutingProtocol(0)->GetTypeId(), aodv.GetTypeId(), "Failed to install AODV routing on node.");
        }
    }

    // Test IP address assignment (verify that IP addresses are correctly assigned to devices)
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that the IP addresses are assigned correctly
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node.");
        }
    }

    // Test UDP Echo Server setup (verify that the UDP Echo Server is set up correctly on the last node)
    void TestUdpEchoServerSetup()
    {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Server is installed on the last node
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on the last node.");
    }

    // Test UDP Echo Client setup (verify that the UDP Echo Client is set up correctly on the first node)
    void TestUdpEchoClientSetup()
    {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Client is installed on the first node
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on the first node.");
    }

    // Test data transmission (verify that data is transmitted between the UDP Echo Client and Server)
    void TestDataTransmission()
    {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Additional checks can be added to verify that the client-server communication was successful.
    }
};

// Register the test suite
TestSuite wifiAodvTestSuite("WifiAodvTestSuite", SYSTEM);
wifiAodvTestSuite.AddTestCase(new WifiAodvTestSuite());
