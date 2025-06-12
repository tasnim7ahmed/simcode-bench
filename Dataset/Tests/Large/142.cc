#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExampleTest");

// Test 1: Verify Node Creation (Central Node + Peripheral Nodes)
void TestNodeCreation()
{
    NodeContainer centralNode;
    centralNode.Create(1); // Central node (n0)
    
    NodeContainer peripheralNodes;
    peripheralNodes.Create(2); // Peripheral nodes (n1, n2)

    // Ensure we have exactly 1 central node and 2 peripheral nodes
    NS_ASSERT_MSG(centralNode.GetN() == 1, "One central node should be created.");
    NS_ASSERT_MSG(peripheralNodes.GetN() == 2, "Two peripheral nodes should be created.");
}

// Test 2: Verify Point-to-Point Link Installation (Central Node to Peripheral Nodes)
void TestPointToPointLinkInstallation()
{
    NodeContainer centralNode;
    centralNode.Create(1);
    
    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    // Ensure that the devices are installed on both sides of each link
    NS_ASSERT_MSG(devicesCentralToNode1.GetN() == 1 && devicesCentralToNode2.GetN() == 1, 
                  "Devices should be installed between the central node and both peripheral nodes.");
}

// Test 3: Verify IP Address Assignment for Links (Central to Node 1 and Central to Node 2)
void TestIpAddressAssignment()
{
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesCentralToNode1, interfacesCentralToNode2;

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfacesCentralToNode1 = address.Assign(devicesCentralToNode1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfacesCentralToNode2 = address.Assign(devicesCentralToNode2);

    // Ensure the interfaces have valid IP addresses assigned
    NS_ASSERT_MSG(interfacesCentralToNode1.GetAddress(0) != Ipv4Address("0.0.0.0"), "Node 1 should have a valid IP address.");
    NS_ASSERT_MSG(interfacesCentralToNode2.GetAddress(0) != Ipv4Address("0.0.0.0"), "Node 2 should have a valid IP address.");
}

// Test 4: Verify OnOff Application Installation on Peripheral Node 1
void TestOnOffApplicationInstallation()
{
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCentralToNode1, interfacesCentralToNode2;

    interfacesCentralToNode1 = address.Assign(devicesCentralToNode1);
    interfacesCentralToNode2 = address.Assign(devicesCentralToNode2);

    uint16_t port = 9; // Port for communication
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesCentralToNode2.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("2Mbps"), 1024); // 2Mbps data rate, 1024-byte packets
    ApplicationContainer app = onoff.Install(peripheralNodes.Get(0)); // Install on Node 1
    app.Start(Seconds(1.0)); // Start at 1 second
    app.Stop(Seconds(10.0)); // Stop at 10 seconds

    // Ensure the OnOff application was installed on Node 1
    NS_ASSERT_MSG(app.GetN() == 1, "The OnOff application should be installed on Node 1.");
}

// Test 5: Verify PacketSink Application Installation on Peripheral Node 2
void TestPacketSinkApplicationInstallation()
{
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCentralToNode1, interfacesCentralToNode2;

    interfacesCentralToNode1 = address.Assign(devicesCentralToNode1);
    interfacesCentralToNode2 = address.Assign(devicesCentralToNode2);

    uint16_t port = 9; // Port for communication
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(peripheralNodes.Get(1)); // Install sink on Node 2
    sinkApp.Start(Seconds(1.0)); // Start at 1 second
    sinkApp.Stop(Seconds(10.0)); // Stop at 10 seconds

    // Ensure the PacketSink application was installed on Node 2
    NS_ASSERT_MSG(sinkApp.GetN() == 1, "The PacketSink application should be installed on Node 2.");
}

// Test 6: Verify Routing Table Population
void TestRoutingTables()
{
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCentralToNode1, interfacesCentralToNode2;

    interfacesCentralToNode1 = address.Assign(devicesCentralToNode1);
    interfacesCentralToNode2 = address.Assign(devicesCentralToNode2);

    // Enable routing in the network
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Check if routing tables are populated for all nodes
    Ptr<Ipv4> ipv4 = centralNode.Get(0)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Central node should have routing entries.");
    
    ipv4 = peripheralNodes.Get(0)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Peripheral node 1 should have routing entries.");

    ipv4 = peripheralNodes.Get(1)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Peripheral node 2 should have routing entries.");
}

// Test 7: Verify Data Flow from Node 1 to Node 2
void TestDataFlow()
{
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCentralToNode1, interfacesCentralToNode2;

    interfacesCentralToNode1 = address.Assign(devicesCentralToNode1);
    interfacesCentralToNode2 = address.Assign(devicesCentralToNode2);

    uint16_t port = 9; // Port for communication
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesCentralToNode2.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("2Mbps"), 1024);
    ApplicationContainer app = onoff.Install(peripheralNodes.Get(0)); // Install on Node 1
    app.Start(Seconds(1.0)); // Start at 1 second
    app.Stop(Seconds(10.0)); // Stop at 10 seconds

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(peripheralNodes.Get(1)); // Install sink on Node 2
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Verify that packets are received by the PacketSink on Node 
