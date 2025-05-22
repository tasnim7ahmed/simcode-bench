#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleWirelessNetwork");

// Test 1: Node creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3);

    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Expected 3 nodes to be created");
}

// Test 2: Wi-Fi configuration and installation
void TestWifiInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);  // Create 3 nodes

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "Expected 3 Wi-Fi devices to be installed");
}

// Test 3: Internet stack installation
void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);  // Create 3 nodes

    InternetStackHelper internet;
    internet.Install(nodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed correctly on node");
    }
}

// Test 4: IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);  // Create 3 nodes

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ipv4Address ip = interfaces.GetAddress(i);
        NS_TEST_ASSERT_MSG_NE(ip, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node");
    }
}

// Test 5: Application installation and configuration
void TestApplicationInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);  // Create 3 nodes

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;  // Port number for echo

    // Source application (CBR)
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer sourceApps = onoff.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // Sink application
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Ensure applications are installed and started at correct time
    NS_TEST_ASSERT_MSG_EQ(sourceApps.Get(0)->GetStartTime().GetSeconds(), 1.0, "Source application did not start at the correct time");
    NS_TEST_ASSERT_MSG_EQ(sinkApps.Get(0)->GetStartTime().GetSeconds(), 1.0, "Sink application did not start at the correct time");
}

// Test 6: Packet transmission
void TestPacketTransmission()
{
    NodeContainer nodes;
    nodes.Create(3);  // Create 3 nodes

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;  // Port number for echo

    // Source application (CBR)
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer sourceApps = onoff.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // Sink application
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Run simulation and check for successful transmission
    Simulator::Run();

    // Verify if data was received (checking packet count)
    Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApps.Get(0));
    NS_TEST_ASSERT_MSG_GT(packetSink->GetTotalBytesReceived(), 0, "No packets received by sink");

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestWifiInstallation();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestPacketTransmission();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
