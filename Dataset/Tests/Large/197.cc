#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PNetworkExample");

// Test 1: Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3);

    // Verify that 3 nodes were created
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Expected 3 nodes");
}

// Test 2: Point-to-Point Link Installation
void TestP2PLinkInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices and channels between node0-node1 and node1-node2
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Verify that the devices were correctly installed
    NS_TEST_ASSERT_MSG_EQ(devices0.GetN(), 1, "Failed to install device between node0 and node1");
    NS_TEST_ASSERT_MSG_EQ(devices1.GetN(), 1, "Failed to install device between node1 and node2");
}

// Test 3: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install the internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Verify that IP addresses have been assigned
    NS_TEST_ASSERT_MSG_NE(interfaces0.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to node0");
    NS_TEST_ASSERT_MSG_NE(interfaces1.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to node1");
}

// Test 4: Routing Table Population
void TestRoutingTablePopulation()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install the internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Enable routing on the middle node (node1)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Verify that routing tables are populated
    Ptr<Ipv4> ipv4 = nodes.Get(1)->GetObject<Ipv4>();
    NS_TEST_ASSERT_MSG_NE(ipv4->GetRoutingProtocol(), nullptr, "Routing protocol not set for node1");
}

// Test 5: TCP Server Installation
void TestTcpServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install the internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Create a TCP server on node2 to receive traffic
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(interfaces1.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Verify server installation
    NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "Failed to install TCP server on node2");
}

// Test 6: TCP Client Installation
void TestTcpClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install the internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Create a TCP client on node0 to send traffic to node2
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(interfaces1.GetAddress(1), port));
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientHelper.SetAttribute("Remote", AddressValue(serverAddress));
    clientApps.Add(clientHelper.Install(nodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Verify client installation
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install TCP client on node0");
}

// Test 7: Packet Reception
void TestPacketReception()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install the internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Create a TCP server on node2 to receive traffic
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(interfaces1.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create a TCP client on node0 to send traffic to node2
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientHelper.SetAttribute("Remote", AddressValue(serverAddress));
    clientApps.Add(clientHelper.Install(nodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();

    // Output received packets
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp.Get(0));
    NS_TEST_ASSERT_MSG_GT(sink1->GetTotalRx(), 0, "No packets received by node2");

    Simulator::Destroy();
}

// Main function to run all tests
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestP2PLinkInstallation();
    TestIpAddressAssignment();
    TestRoutingTablePopulation();
    TestTcpServerInstallation();
    TestTcpClientInstallation();
    TestPacketReception();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
