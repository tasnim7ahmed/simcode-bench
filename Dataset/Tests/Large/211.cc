#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiSimulationTests");

// Test 1: Node Creation
void TestNodeCreation()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Verify that 5 nodes were created
    NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 5, "Expected 5 Wi-Fi nodes");
}

// Test 2: Wi-Fi Device Installation
void TestWifiDeviceInstallation()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up Wi-Fi PHY and MAC for ad-hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

    // Verify that Wi-Fi devices were installed on all nodes
    NS_TEST_ASSERT_MSG_EQ(wifiDevices.GetN(), 5, "Failed to install Wi-Fi devices on all nodes");
}

// Test 3: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up Wi-Fi devices and install Internet stack
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_NE(wifiInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to node 0");
}

// Test 4: Mobility Model Installation
void TestMobilityModelInstallation()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(wifiNodes);

    // Verify mobility model installation
    Ptr<RandomWalk2dMobilityModel> model = wifiNodes.Get(0)->GetObject<RandomWalk2dMobilityModel>();
    NS_TEST_ASSERT_MSG_NE(model, nullptr, "Failed to install mobility model on node 0");
}

// Test 5: UDP Server Installation
void TestUdpServerInstallation()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up UDP server on node 1
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Verify UDP server installation
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on node 1");
}

// Test 6: UDP Client Installation
void TestUdpClientInstallation()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up UDP client on node 0
    uint16_t port = 4000;
    UdpClientHelper udpClient(wifiNodes.Get(1)->GetObject<Ipv4>()->GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify UDP client installation
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client on node 0");
}

// Test 7: FlowMonitor Setup
void TestFlowMonitorSetup()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up Wi-Fi devices and install Internet stack
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);

    // Set up FlowMonitor to track traffic
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Verify FlowMonitor installation
    NS_TEST_ASSERT_MSG_NE(monitor, nullptr, "Failed to install FlowMonitor");
}

// Test 8: FlowMonitor Statistics Collection
void TestFlowMonitorStatistics()
{
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Set up Wi-Fi devices and install Internet stack
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices);

    // Set up UDP applications (server on node 1 and client on node 0)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(wifiInterfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up FlowMonitor to track traffic
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Collect FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    // Verify flow stats collection
    NS_TEST_ASSERT_MSG_GT(stats.size(), 0, "FlowMonitor did not track any flows");

    // Clean up and exit
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Call each test function
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestIpAddressAssignment();
    TestMobilityModelInstallation();
    TestUdpServerInstallation();
    TestUdpClientInstallation();
    TestFlowMonitorSetup();
    TestFlowMonitorStatistics();

    return 0;
}
