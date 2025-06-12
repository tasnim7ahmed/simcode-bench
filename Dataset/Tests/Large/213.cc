#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExampleTests");

// Test 1: Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Verify that 2 nodes are created
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Expected 2 nodes");
}

// Test 2: Point-to-Point Device Installation
void TestPointToPointDeviceInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify that devices are installed
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install point-to-point devices on both nodes");
}

// Test 3: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to node 0");
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Failed to assign IP to node 1");
}

// Test 4: TCP Server Application Installation
void TestTcpServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create a TCP server on node 1
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Verify that the server application is installed
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install TCP Server on node 1");
}

// Test 5: TCP Client Application Installation
void TestTcpClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create a TCP client on node 0
    uint16_t port = 8080;
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.2"), port)));
    clientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify that the client application is installed
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install TCP Client on node 0");
}

// Test 6: Flow Monitor Setup
void TestFlowMonitorSetup()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a TCP server and client applications
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    clientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Verify flow monitor setup
    NS_TEST_ASSERT_MSG_NE(monitor, nullptr, "Flow monitor is not installed");
}

// Test 7: Flow Statistics Collection
void TestFlowStatisticsCollection()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a TCP server and client applications
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    clientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Check flow stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier()));
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    // Verify flow statistics collection
    NS_TEST_ASSERT_MSG_GT(stats.size(), 0, "FlowMonitor did not collect flow stats");

    // Clean up and exit
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Call each test function
    TestNodeCreation();
    TestPointToPointDeviceInstallation();
    TestIpAddressAssignment();
    TestTcpServerInstallation();
    TestTcpClientInstallation();
    TestFlowMonitorSetup();
    TestFlowStatisticsCollection();

    return 0;
}
