#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoThreeNodesTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3); // Create 3 nodes
    NS_ASSERT_MSG(nodes.GetN() == 3, "Exactly three nodes should be created.");
}

// Test 2: Verify Point-to-Point Link Installation
void TestPointToPointLinkInstallation()
{
    NodeContainer nodes;
    nodes.Create(3); // Create 3 nodes

    // Set up the point-to-point link between nodes
    PointToPointHelper pointToPoint1, pointToPoint2;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes and the links between them
    NetDeviceContainer devices1, devices2;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); // Link n0 -> n1
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2)); // Link n1 -> n2

    // Check if the devices are properly installed
    NS_ASSERT_MSG(devices1.GetN() == 1 && devices2.GetN() == 1, "Each point-to-point link should have one device.");
}

// Test 3: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);

    // Set up the point-to-point link
    PointToPointHelper pointToPoint1, pointToPoint2;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes
    NetDeviceContainer devices1, devices2;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); 
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    
    // Assign IP address for the link between n0 and n1
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Assign IP address for the link between n1 and n2
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    // Check if interfaces have valid IP addresses
    NS_ASSERT_MSG(interfaces1.GetAddress(0) != Ipv4Address("0.0.0.0"), "Node n0 should have a valid IP address.");
    NS_ASSERT_MSG(interfaces2.GetAddress(1) != Ipv4Address("0.0.0.0"), "Node n2 should have a valid IP address.");
}

// Test 4: Verify Routing Table Population
void TestRoutingTablePopulation()
{
    NodeContainer nodes;
    nodes.Create(3);

    // Set up the point-to-point links
    PointToPointHelper pointToPoint1, pointToPoint2;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and Internet stack
    NetDeviceContainer devices1, devices2;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1));
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    
    // Assign IP addresses
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    // Enable routing on node n1 (acting as router)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simulate route population
    Ptr<Ipv4> ipv4_1 = nodes.Get(1)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4_1->GetRoutingTableEntryCount() > 0, "Node n1 should have routing entries.");
}

// Test 5: Verify UDP Echo Server Installation on Node n2
void TestUdpEchoServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    uint16_t port = 9; // Echo server port number
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2)); // Server is node n2
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on node n2.");
}

// Test 6: Verify UDP Echo Client Installation on Node n0
void TestUdpEchoClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    // Set up the point-to-point links and IP addresses
    PointToPointHelper pointToPoint1, pointToPoint2;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1, devices2;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1));
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    uint16_t port = 9;
    UdpEchoClientHelper echoClient(interfaces2.GetAddress(1), port); // Server IP (n2)
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // Client is node n0
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed on node n0.");
}

// Run all tests
int main(int argc, char *argv[])
{
    NS_LOG_INFO("Running ns-3 UDP Echo Three Nodes Unit Tests");

    TestNodeCreation();
    TestPointToPointLinkInstallation();
    TestIpAddressAssignment();
    TestRoutingTablePopulation();
    TestUdpEchoServerInstallation();
    TestUdpEchoClientInstallation();

    NS_LOG_INFO("All tests passed successfully.");
    return 0;
}
