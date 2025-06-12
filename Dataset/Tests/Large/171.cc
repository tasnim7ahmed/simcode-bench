#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

// Test for Node creation
void TestNodeCreation()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    NS_TEST_ASSERT_MSG_EQ(adhocNodes.GetN(), numNodes, "Failed to create the expected number of nodes.");
}

// Test for Wi-Fi device installation
void TestWifiDeviceInstallation()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.0));
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, adhocNodes);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), numNodes, "Failed to install Wi-Fi devices on nodes.");
}

// Test for Mobility model installation
void TestMobilityInstallation()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    MobilityHelper mobility;
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(0.0));
    speed->SetAttribute("Max", DoubleValue(20.0));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", PointerValue(speed),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"),
                              "X", DoubleValue(500.0),
                              "Y", DoubleValue(500.0));

    mobility.Install(adhocNodes);

    NS_TEST_ASSERT_MSG_EQ(adhocNodes.GetN(), numNodes, "Failed to install mobility model on nodes.");
}

// Test for AODV Routing protocol installation
void TestAodvRoutingInstallation()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // AODV as the routing protocol
    internet.Install(adhocNodes);

    Ptr<Ipv4> ipv4 = adhocNodes.Get(0)->GetObject<Ipv4>();
    NS_TEST_ASSERT_MSG_EQ(ipv4->GetRoutingProtocol()->GetTypeId(), aodv.GetTypeId(), "AODV routing protocol not installed.");
}

// Test for IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.0));
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, adhocNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), numNodes, "Failed to assign IP addresses to nodes.");
}

// Test for UDP Echo Server application installation
void TestUdpEchoServerApplication()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(adhocNodes.Get(0)); // Server on node 0
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(30.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install the UDP Echo Server application.");
}

// Test for UDP Echo Client application installation
void TestUdpEchoClientApplication()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    uint16_t port = 9;
    UdpEchoClientHelper echoClient("10.1.1.1", port); // Client to node 0
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(adhocNodes.Get(numNodes - 1)); // Client on last node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(30.0));

    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install the UDP Echo Client application.");
}

// Test for Flow Monitor setup and flow statistics collection
void TestFlowMonitor()
{
    NodeContainer adhocNodes;
    uint32_t numNodes = 10;
    adhocNodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.0));
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, adhocNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(adhocNodes.Get(0)); // Server on node 0
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port); // Client to node 0
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(adhocNodes.Get(numNodes - 1)); // Client on last node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(30.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    NS_TEST_ASSERT_MSG_EQ(stats.size(), 1, "Flow Monitor did not capture the expected number of flows.");
    Simulator::Destroy();
}

// Main function to execute all tests
int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestMobilityInstallation();
    TestAodvRoutingInstallation();
    TestIpAddressAssignment();
    TestUdpEchoServerApplication();
    TestUdpEchoClientApplication();
    TestFlowMonitor();

    return 0;
}
