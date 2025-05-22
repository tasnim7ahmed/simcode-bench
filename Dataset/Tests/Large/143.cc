#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExampleTest");

// Test 1: Verify Node Creation (Root, Child, and Leaf Nodes)
void TestNodeCreation()
{
    NodeContainer rootNode;
    rootNode.Create(1); // Root node

    NodeContainer childNodes;
    childNodes.Create(2); // Two child nodes

    NodeContainer leafNode;
    leafNode.Create(1); // One leaf node

    // Ensure we have the correct number of nodes
    NS_ASSERT_MSG(rootNode.GetN() == 1, "One root node should be created.");
    NS_ASSERT_MSG(childNodes.GetN() == 2, "Two child nodes should be created.");
    NS_ASSERT_MSG(leafNode.GetN() == 1, "One leaf node should be created.");
}

// Test 2: Verify Point-to-Point Link Installation (Root to Child, Child to Leaf)
void TestPointToPointLinkInstallation()
{
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    // Set up links
    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0)); // Root to Child 1
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1)); // Root to Child 2
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0)); // Child 1 to Leaf

    // Ensure devices are installed on each link
    NS_ASSERT_MSG(devicesRootToChild1.GetN() == 1, "Device should be installed between root and child 1.");
    NS_ASSERT_MSG(devicesRootToChild2.GetN() == 1, "Device should be installed between root and child 2.");
    NS_ASSERT_MSG(devicesChildToLeaf.GetN() == 1, "Device should be installed between child and leaf.");
}

// Test 3: Verify IP Address Assignment for Links
void TestIpAddressAssignment()
{
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0)); // Root to Child 1
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1)); // Root to Child 2
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0)); // Child to Leaf

    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(childNodes);
    stack.Install(leafNode);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesRootToChild1, interfacesRootToChild2, interfacesChildToLeaf;

    // Assign IP addresses to links
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfacesRootToChild1 = address.Assign(devicesRootToChild1);
    
    address.SetBase("10.1.2.0", "255.255.255.0");
    interfacesRootToChild2 = address.Assign(devicesRootToChild2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    interfacesChildToLeaf = address.Assign(devicesChildToLeaf);

    // Ensure IP addresses have been assigned
    NS_ASSERT_MSG(interfacesRootToChild1.GetAddress(0) != Ipv4Address("0.0.0.0"), "Root to Child 1 should have a valid IP address.");
    NS_ASSERT_MSG(interfacesRootToChild2.GetAddress(0) != Ipv4Address("0.0.0.0"), "Root to Child 2 should have a valid IP address.");
    NS_ASSERT_MSG(interfacesChildToLeaf.GetAddress(0) != Ipv4Address("0.0.0.0"), "Child to Leaf should have a valid IP address.");
}

// Test 4: Verify OnOff Application Installation on Leaf Node
void TestOnOffApplicationInstallation()
{
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0)); // Root to Child 1
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1)); // Root to Child 2
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0)); // Child to Leaf

    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(childNodes);
    stack.Install(leafNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesRootToChild1, interfacesRootToChild2, interfacesChildToLeaf;

    interfacesRootToChild1 = address.Assign(devicesRootToChild1);
    interfacesRootToChild2 = address.Assign(devicesRootToChild2);
    interfacesChildToLeaf = address.Assign(devicesChildToLeaf);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesRootToChild1.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 1024);
    ApplicationContainer app = onoff.Install(leafNode.Get(0)); // Install on Leaf node
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Ensure the OnOff application was installed on the Leaf node
    NS_ASSERT_MSG(app.GetN() == 1, "The OnOff application should be installed on the Leaf node.");
}

// Test 5: Verify PacketSink Application Installation on Root Node
void TestPacketSinkApplicationInstallation()
{
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0)); // Root to Child 1
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1)); // Root to Child 2
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0)); // Child to Leaf

    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(childNodes);
    stack.Install(leafNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesRootToChild1, interfacesRootToChild2, interfacesChildToLeaf;

    interfacesRootToChild1 = address.Assign(devicesRootToChild1);
    interfacesRootToChild2 = address.Assign(devicesRootToChild2);
    interfacesChildToLeaf = address.Assign(devicesChildToLeaf);

    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(rootNode.Get(0)); // Install on Root node
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Ensure the PacketSink application was installed on the Root node
    NS_ASSERT_MSG(sinkApp.GetN() == 1, "The PacketSink application should be installed on the Root node.");
}

// Test 6: Verify Routing Table Population
void TestRoutingTables()
{
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0)); // Root to Child 1
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1)); // Root to Child 2
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0)); // Child to Leaf

    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(childNodes);
    stack.Install(leafNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesRootToChild1, interfacesRootToChild2, interfacesChildToLeaf;

    interfacesRootToChild1 = address.Assign(devicesRootToChild1);
    interfacesRootToChild2 = address.Assign(devicesRootToChild2);
    interfacesChildToLeaf = address.Assign(devicesChildToLeaf);

    // Enable routing in the network
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Ensure routing tables are populated for all nodes
    Ptr<Ipv4> ipv4 = rootNode.Get(0)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Root node should have routing entries.");
    
    ipv4 = childNodes.Get(0)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Child node should have routing entries.");

    ipv4 = leafNode.Get(0)->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Leaf node should have routing entries.");
}

// Test 7: Verify Data Flow from Leaf to Root
void TestDataFlow()
{
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0)); // Root to Child 1
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1)); // Root to Child 2
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0)); // Child to Leaf

    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(childNodes);
    stack.Install(leafNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesRootToChild1, interfacesRootToChild2, interfacesChildToLeaf;

    interfacesRootToChild1 = address.Assign(devicesRootToChild1);
    interfacesRootToChild2 = address.Assign(devicesRootToChild2);
    interfacesChildToLeaf = address.Assign(devicesChildToLeaf);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesRootToChild1.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 1024);
    ApplicationContainer app = onoff.Install(leafNode.Get(0)); // Install on Leaf node
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(rootNode.Get(0)); // Install on Root node
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Simulator::Run();

    Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));
    NS_ASSERT_MSG(sinkPtr->GetTotalBytesReceived() > 0, "No packets received at the Root node.");

    Simulator::Destroy();
}

int main()
{
    TestNodeCreation();
    TestPointToPointLinkInstallation();
    TestIpAddressAssignment();
    TestOnOffApplicationInstallation();
    TestPacketSinkApplicationInstallation();
    TestRoutingTables();
    TestDataFlow();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}

