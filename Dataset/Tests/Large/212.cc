#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AODVAdhocNetworkTests");

// Test 1: Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Verify that 5 nodes are created
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 5, "Expected 5 nodes");
}

// Test 2: Wi-Fi Device Installation
void TestWifiDeviceInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up wireless channel and PHY layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi (non-infrastructure mode)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Verify that Wi-Fi devices are installed on all nodes
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 5, "Failed to install Wi-Fi devices on all nodes");
}

// Test 3: Mobility Model Installation
void TestMobilityModelInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set mobility model (random placement of nodes)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Verify that mobility model is installed on nodes
    Ptr<ConstantPositionMobilityModel> model = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    NS_TEST_ASSERT_MSG_NE(model, nullptr, "Failed to install mobility model on node 0");
}

// Test 4: AODV Routing Protocol Installation
void TestAodvRoutingProtocol()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up AODV routing protocol
    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Verify that AODV is correctly installed
    Ptr<Ipv4RoutingProtocol> routingProtocol = nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol();
    NS_TEST_ASSERT_MSG_NE(routingProtocol, nullptr, "AODV routing protocol not installed on node 0");
}

// Test 5: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up Wi-Fi devices and install Internet stack with AODV routing protocol
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to node 0");
}

// Test 6: UDP Echo Server Installation
void TestUdpEchoServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up UDP Echo Server on node 1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Verify UDP Echo Server installation
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on node 1");
}

// Test 7: UDP Echo Client Installation
void TestUdpEchoClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up UDP Echo Client on node 0 to send packets to node 1
    UdpEchoClientHelper echoClient("10.1.1.2", 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Verify UDP Echo Client installation
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on node 0");
}

// Test 8: Flow Monitor Setup
void TestFlowMonitorSetup()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up Wi-Fi devices and install Internet stack with AODV routing protocol
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Verify flow monitor setup
    NS_TEST_ASSERT_MSG_NE(monitor, nullptr, "Flow monitor is not installed");
}

// Test 9: Flow Monitor Statistics Collection
void TestFlowMonitorStatistics()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Set up Wi-Fi devices and install Internet stack with AODV routing protocol
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP Echo Server on node 1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Check flow stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    // Verify flow stats collection
    NS_TEST_ASSERT_MSG_GT(stats.size(), 0, "FlowMonitor did not collect flow stats");

    // Clean up and exit
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Call each test function
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestMobilityModelInstallation();
    TestAodvRoutingProtocol();
    TestIpAddressAssignment();
    TestUdpEchoServerInstallation();
    TestUdpEchoClientInstallation();
    TestFlowMonitorSetup();
    TestFlowMonitorStatistics();

    return 0;
}
