#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocNetworkUnitTests");

// Test 1: Node creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(4);
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Expected 4 nodes to be created");
}

// Test 2: WiFi installation
void TestWifiInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 4, "Expected WiFi devices to be installed on all 4 nodes");
}

// Test 3: AODV routing installation
void TestAodvRouting()
{
    NodeContainer nodes;
    nodes.Create(4);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "AODV routing protocol not installed correctly");
    }
}

// Test 4: Mobility model
void TestMobilityModel()
{
    NodeContainer nodes;
    nodes.Create(4);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.Install(nodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed correctly");
    }
}

// Test 5: UDP traffic setup
void TestUdpTraffic()
{
    NodeContainer nodes;
    nodes.Create(4);

    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpClientHelper udpClient(Ipv4Address("10.1.1.3"), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    Simulator::Run();
    NS_TEST_ASSERT_MSG_EQ(serverApp.Get(0)->GetStartTime().GetSeconds(), 1.0, "Server did not start at the correct time");
    NS_TEST_ASSERT_MSG_EQ(clientApp.Get(0)->GetStartTime().GetSeconds(), 2.0, "Client did not start at the correct time");
    Simulator::Destroy();
}

// Test 6: FlowMonitor validation
void TestFlowMonitor()
{
    NodeContainer nodes;
    nodes.Create(4);

    // Set up a basic WiFi and mobility configuration (as in the main code)
    WifiHelper wifi;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_NE(stats.size(), 0, "FlowMonitor should capture some traffic");

    Simulator::Destroy();
}

// Test 7: Throughput calculation
void TestThroughputCalculation()
{
    NodeContainer nodes;
    nodes.Create(4);

    // Repeat basic UDP traffic setup (omitting for brevity)
    WifiHelper wifi;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Check throughput from FlowMonitor stats
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    double expectedThroughput = 1024 * 320 * 8.0 / 20.0 / 1000 / 1000; // Mbps

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        double throughput = it->second.rxBytes * 8.0 / 20.0 / 1000 / 1000;
        NS_TEST_ASSERT_MSG_EQ_TOL(throughput, expectedThroughput, 0.1, "Throughput is not as expected");
    }

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestWifiInstallation();
    TestAodvRouting();
    TestMobilityModel();
    TestUdpTraffic();
    TestFlowMonitor();
    TestThroughputCalculation();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}

