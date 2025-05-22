#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

// Test for node creation (AP + STAs)
void TestNodeCreation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Failed to create 2 STA nodes.");
    NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create 1 AP node.");
}

// Test for Wi-Fi device installation (STA + AP devices)
void TestWifiDeviceInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Configure STA devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Failed to install Wi-Fi devices on STA nodes.");
    NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on AP node.");
}

// Test for mobility model installation on nodes (STA and AP)
void TestMobilityInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Check mobility models are installed correctly
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.Get(0)->GetObject<MobilityModel>(), nullptr, "Mobility model not installed on STA1.");
    NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.Get(1)->GetObject<MobilityModel>(), nullptr, "Mobility model not installed on STA2.");
    NS_TEST_ASSERT_MSG_EQ(wifiApNode.Get(0)->GetObject<MobilityModel>(), nullptr, "Mobility model not installed on AP.");
}

// Test for IP address assignment to devices
void TestIpAddressAssignment()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    NS_TEST_ASSERT_MSG_EQ(staInterfaces.GetN(), 2, "Failed to assign IP addresses to STA nodes.");
    NS_TEST_ASSERT_MSG_EQ(apInterface.GetN(), 1, "Failed to assign IP address to AP node.");
}

// Test for the PacketSink application (TCP server) installation
void TestPacketSinkApplication()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 9; // TCP port
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNodes.Get(1)); // Server on STA2
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "Failed to install the PacketSink application.");
}

// Test for the BulkSend (TCP client) application installation
void TestBulkSendApplication()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 9; // TCP port
    Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(1), port));
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(wifiStaNodes.Get(0)); // Client on STA1
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install the BulkSend application.");
}

// Test for FlowMonitor statistics (Throughput, Rx, Tx)
void TestFlowMonitor()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 9; // TCP port
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNodes.Get(1)); // Server on STA2
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(wifiStaNodes.Get(0)); // Client on STA1
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    NS_TEST_ASSERT_MSG_EQ(stats.size(), 1, "Flow monitor did not capture the expected number of flows.");
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestMobilityInstallation();
    TestIpAddressAssignment();
    TestPacketSinkApplication();
    TestBulkSendApplication();
    TestFlowMonitor();

    return 0;
}
