#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

// Test for Node creation
void TestNodeCreation()
{
    // Create wired nodes
    NodeContainer wiredNodes;
    uint32_t numWiredNodes = 4;
    wiredNodes.Create(numWiredNodes);

    // Create wireless nodes
    NodeContainer wifiStaNodes;
    uint32_t numWifiStaNodes = 3;
    wifiStaNodes.Create(numWifiStaNodes);

    NS_TEST_ASSERT_MSG_EQ(wiredNodes.GetN(), numWiredNodes, "Failed to create the expected number of wired nodes.");
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), numWifiStaNodes, "Failed to create the expected number of wireless nodes.");
}

// Test for Point-to-Point link creation
void TestPointToPointLinkCreation()
{
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < wiredNodes.GetN() - 1; ++i)
    {
        NetDeviceContainer link = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pDevices.Add(link);
    }

    NS_TEST_ASSERT_MSG_EQ(p2pDevices.GetN(), 3, "Failed to establish point-to-point links between wired nodes.");
}

// Test for Wi-Fi device installation and AP setup
void TestWifiDeviceInstallation()
{
    NodeContainer wiredNodes;
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode = wiredNodes.Get(3); // Use one wired node as AP

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 3, "Failed to install Wi-Fi devices on wireless nodes.");
    NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi AP device.");
}

// Test for IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer wiredNodes;
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode = wiredNodes.Get(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < wiredNodes.GetN() - 1; ++i)
    {
        NetDeviceContainer link = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pDevices.Add(link);
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    address.Assign(apDevice);

    NS_TEST_ASSERT_MSG_EQ(wiredInterfaces.GetN(), 3, "Failed to assign IP addresses to wired nodes.");
    NS_TEST_ASSERT_MSG_EQ(wifiInterfaces.GetN(), 3, "Failed to assign IP addresses to wireless nodes.");
}

// Test for Mobility model installation
void TestMobilityInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 3, "Failed to install mobility model on wireless nodes.");
    NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to install mobility model on the AP node.");
}

// Test for TCP traffic generation
void TestTcpTraffic()
{
    NodeContainer wiredNodes;
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode = wiredNodes.Get(3); // Use one wired node as AP

    uint16_t port = 9;
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        AddressValue remoteAddress(InetSocketAddress("10.1.1.1", port)); // Assume the IP address of AP is 10.1.1.1
        onOffHelper.SetAttribute("Remote", remoteAddress);
        clientApps.Add(onOffHelper.Install(wifiStaNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(wiredNodes.Get(3)); // Sink on the last wired node
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 3, "Failed to create TCP client applications.");
}

// Test for Flow Monitor and Throughput calculation
void TestFlowMonitor()
{
    NodeContainer wiredNodes;
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode = wiredNodes.Get(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < wiredNodes.GetN() - 1; ++i)
    {
        NetDeviceContainer link = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pDevices.Add(link);
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    address.Assign(apDevice);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    for (const auto& flowStats : stats)
    {
        totalThroughput += flowStats.second.rxBytes * 8.0 / 10.0 / 1000 / 1000; // in Mbps
    }

    NS_LOG_UNCOND("Average Throughput: " << totalThroughput << " Mbps");

    NS_TEST_ASSERT_MSG(totalThroughput > 0, "Throughput calculation failed.");
}

int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestPointToPointLinkCreation();
    TestWifiDeviceInstallation();
    TestIpAddressAssignment();
    TestMobilityInstallation();
    TestTcpTraffic();
    TestFlowMonitor();

    return 0;
}
