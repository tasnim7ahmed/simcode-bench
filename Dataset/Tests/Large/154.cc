#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DatasetExampleTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(5);

    // Ensure 5 nodes are created
    NS_ASSERT_MSG(nodes.GetN() == 5, "Five nodes should be created.");
}

// Test 2: Verify WiFi Devices Installation
void TestWifiDevicesInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Ensure WiFi devices are installed on all 5 nodes
    NS_ASSERT_MSG(devices.GetN() == 5, "Five WiFi devices should be installed.");
}

// Test 3: Verify Mobility Model Configuration
void TestMobilityModelConfiguration()
{
    NodeContainer nodes;
    nodes.Create(5);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Verify that mobility models are applied to all nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
        NS_ASSERT_MSG(mobilityModel != nullptr, "Mobility model should be installed on node " + std::to_string(i));
    }
}

// Test 4: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(5);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Ensure IP addresses are assigned correctly
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        NS_ASSERT_MSG(interfaces.GetAddress(i) != Ipv4Address("0.0.0.0"), "IP address not assigned for node " + std::to_string(i));
    }
}

// Test 5: Verify UDP Echo Server Installation on Node 4
void TestUdpEchoServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    UdpEchoServerHelper echoServer(9);  // Port number 9
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Ensure the UDP Echo Server is installed on node 4
    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on node 4.");
}

// Test 6: Verify UDP Echo Clients Installation on Nodes 0, 1, 2, and 3
void TestUdpEchoClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install the UDP Echo clients on nodes 0, 1, 2, and 3
    for (int i = 0; i < 4; i++)
    {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);  // Address of node 4
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Ensure UDP Echo clients are installed on all nodes except node 4
    for (int i = 0; i < 4; i++)
    {
        Ptr<Application> app = nodes.Get(i)->GetApplication(0);
        NS_ASSERT_MSG(app != nullptr, "UDP Echo Client should be installed on node " + std::to_string(i));
    }
}

// Test 7: Verify Flow Monitor Installation
void TestFlowMonitorInstallation()
{
    NodeContainer nodes;
    nodes.Create(5);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    WifiMacHelper mac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Verify Flow Monitor is installed
    NS_ASSERT_MSG(flowMonitor != nullptr, "Flow monitor should be installed.");
}

// Test 8: Verify Packet Data Recording to CSV File
void TestPacketDataRecording()
{
    // Create a dummy flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // Record packet data into a CSV file
    std::string filename = "packet_dataset.csv";
    RecordPacketData(flowMonitor, filename);

    // Check if the CSV file exists and contains data (basic check)
    std::ifstream inFile(filename);
    NS_ASSERT_MSG(inFile.is_open(), "CSV file should be created and openable.");
    inFile.close();
}

int main(int argc, char *argv[])
{
    // Run individual tests
    TestNodeCreation();
    TestWifiDevicesInstallation();
    TestMobilityModelConfiguration();
    TestIpAddressAssignment();
    TestUdpEchoServerInstallation();
    TestUdpEchoClientInstallation();
    TestFlowMonitorInstallation();
    TestPacketDataRecording();

    return 0;
}
