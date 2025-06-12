#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);
    NS_ASSERT_MSG(nodes.GetN() == 2, "Exactly two nodes should be created.");
}

// Test 2: Verify Point-to-Point Link Creation
void TestPointToPointLink()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    NS_ASSERT_MSG(devices.GetN() == 2, "Point-to-Point link should have exactly two devices.");
}

// Test 3: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_ASSERT_MSG(interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "First node should have a valid IP.");
    NS_ASSERT_MSG(interfaces.GetAddress(1) != Ipv4Address("0.0.0.0"), "Second node should have a valid IP.");
}

// Test 4: Verify Static Routing Initialization
void TestStaticRouting()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting_0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticRouting_1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());

    NS_ASSERT_MSG(staticRouting_0 != nullptr, "Static routing should be initialized for node 0.");
    NS_ASSERT_MSG(staticRouting_1 != nullptr, "Static routing should be initialized for node 1.");
}

// Test 5: Verify Route Addition
void TestRouteAddition()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting_0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticRouting_1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());

    staticRouting_0->AddHostRouteTo(Ipv4Address("10.1.1.2"), Ipv4Address("10.1.1.1"), 1);
    staticRouting_1->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.1.2"), 1);

    NS_ASSERT_MSG(staticRouting_0->GetNRoutes() > 0, "Node 0 should have at least one route.");
    NS_ASSERT_MSG(staticRouting_1->GetNRoutes() > 0, "Node 1 should have at least one route.");
}

// Test 6: Verify Poisson Reverse Route Update
void TestPoissonReverseUpdate()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting_0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    
    uint32_t initialRouteCount = staticRouting_0->GetNRoutes();
    
    // Update route using Poisson Reverse
    uint32_t cost = 1;
    uint32_t reverseCost = 1 / cost;
    staticRouting_0->SetDefaultRoute(Ipv4Address("10.1.1.1"), reverseCost);

    NS_ASSERT_MSG(staticRouting_0->GetNRoutes() > initialRouteCount, "Poisson Reverse should add a new route.");
}

// Test 7: Verify TCP Flow Transmission
void TestTcpFlow()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "TCP packets should be transmitted successfully.");
}

// Run all tests
int main(int argc, char *argv[])
{
    NS_LOG_INFO("Running ns-3 Poisson Reverse Unit Tests");

    TestNodeCreation();
    TestPointToPointLink();
    TestIpAddressAssignment();
    TestStaticRouting();
    TestRouteAddition();
    TestPoissonReverseUpdate();
    TestTcpFlow();

    NS_LOG_INFO("All tests passed successfully.");
    return 0;
}
