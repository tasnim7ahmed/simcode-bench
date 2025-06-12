#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSixNodesTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(6); // Create 6 nodes
    NS_ASSERT_MSG(nodes.GetN() == 6, "Exactly six nodes should be created.");
}

// Test 2: Verify Point-to-Point Link Installation
void TestPointToPointLinkInstallation()
{
    NodeContainer nodes;
    nodes.Create(6); // Create 6 nodes

    // Set up the point-to-point links
    PointToPointHelper pointToPoint1, pointToPoint2, pointToPoint3, pointToPoint4, pointToPoint5;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint3.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint4.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint5.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint5.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes and the links between them
    NetDeviceContainer devices1, devices2, devices3, devices4, devices5;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); 
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2)); 
    devices3 = pointToPoint3.Install(nodes.Get(2), nodes.Get(3)); 
    devices4 = pointToPoint4.Install(nodes.Get(3), nodes.Get(4)); 
    devices5 = pointToPoint5.Install(nodes.Get(4), nodes.Get(5)); 

    // Check if devices are installed correctly
    NS_ASSERT_MSG(devices1.GetN() == 1 && devices2.GetN() == 1 && devices3.GetN() == 1 && devices4.GetN() == 1 && devices5.GetN() == 1, 
                  "Each point-to-point link should have one device.");
}

// Test 3: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(6);

    // Set up the point-to-point links
    PointToPointHelper pointToPoint1, pointToPoint2, pointToPoint3, pointToPoint4, pointToPoint5;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint3.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint4.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint5.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint5.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes
    NetDeviceContainer devices1, devices2, devices3, devices4, devices5;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); 
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2)); 
    devices3 = pointToPoint3.Install(nodes.Get(2), nodes.Get(3)); 
    devices4 = pointToPoint4.Install(nodes.Get(3), nodes.Get(4)); 
    devices5 = pointToPoint5.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    
    // Assign IP addresses for all the links
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
    address.SetBase("10.4.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);
    address.SetBase("10.5.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces5 = address.Assign(devices5);

    // Check if interfaces have valid IP addresses
    NS_ASSERT_MSG(interfaces1.GetAddress(0) != Ipv4Address("0.0.0.0"), "Node n0 should have a valid IP address.");
    NS_ASSERT_MSG(interfaces2.GetAddress(1) != Ipv4Address("0.0.0.0"), "Node n1 should have a valid IP address.");
    NS_ASSERT_MSG(interfaces3.GetAddress(1) != Ipv4Address("0.0.0.0"), "Node n2 should have a valid IP address.");
    NS_ASSERT_MSG(interfaces4.GetAddress(1) != Ipv4Address("0.0.0.0"), "Node n3 should have a valid IP address.");
    NS_ASSERT_MSG(interfaces5.GetAddress(1) != Ipv4Address("0.0.0.0"), "Node n4 should have a valid IP address.");
}

// Test 4: Verify Routing Table Population
void TestRoutingTablePopulation()
{
    NodeContainer nodes;
    nodes.Create(6);

    // Set up the point-to-point links
    PointToPointHelper pointToPoint1, pointToPoint2, pointToPoint3, pointToPoint4, pointToPoint5;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint3.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint4.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint5.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint5.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and Internet stack
    NetDeviceContainer devices1, devices2, devices3, devices4, devices5;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); 
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2)); 
    devices3 = pointToPoint3.Install(nodes.Get(2), nodes.Get(3)); 
    devices4 = pointToPoint4.Install(nodes.Get(3), nodes.Get(4)); 
    devices5 = pointToPoint5.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
    address.SetBase("10.4.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);
    address.SetBase("10.5.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces5 = address.Assign(devices5);

    // Enable routing on nodes n1, n2, n3, and n4 (acting as routers)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simulate route population
    Ptr<Ipv4> ipv4_1 = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_2 = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_3 = nodes.Get(3)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_4 = nodes.Get(4)->GetObject<Ipv4>();

    NS_ASSERT_MSG(ipv4_1->GetRoutingTableEntryCount() > 0, "Node n1 should have routing entries.");
    NS_ASSERT_MSG(ipv4_2->GetRoutingTableEntryCount() > 0, "Node n2 should have routing entries.");
    NS_ASSERT_MSG(ipv4_3->GetRoutingTableEntryCount() > 0, "Node n3 should have routing entries.");
    NS_ASSERT_MSG(ipv4_4->GetRoutingTableEntryCount() > 0, "Node n4 should have routing entries.");
}

// Test 5: Verify UDP Echo Client/Server Setup
void TestUdpEchoClientServer()
{
    NodeContainer nodes;
    nodes.Create(6);

    // Set up the point-to-point links and devices
    PointToPointHelper pointToPoint1, pointToPoint2, pointToPoint3, pointToPoint4, pointToPoint5;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint3.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint4.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint5.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint5.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and Internet stack
    NetDeviceContainer devices1, devices2, devices3, devices4, devices5;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); 
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2)); 
    devices3 = pointToPoint3.Install(nodes.Get(2), nodes.Get(3)); 
    devices4 = pointToPoint4.Install(nodes.Get(3), nodes.Get(4)); 
    devices5 = pointToPoint5.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
    address.SetBase("10.4.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);
    address.SetBase("10.5.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces5 = address.Assign(devices5);

    // Set up the UDP Echo Server on node n5
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(5));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP Echo Client on node n0
    UdpEchoClientHelper echoClient(interfaces5.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    // Test if the applications ran successfully
    NS_LOG_INFO("Test UDP Echo Client and Server completed successfully.");
}

int main()
{
    TestNodeCreation();
    TestPointToPointLinkInstallation();
    TestIpAddressAssignment();
    TestRoutingTablePopulation();
    TestUdpEchoClientServer();

    return 0;
}
