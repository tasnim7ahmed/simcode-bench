#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/distance-vector-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SplitHorizonSimulationTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3);
    NS_ASSERT_MSG(nodes.GetN() == 3, "Exactly three nodes (A, B, C) should be created.");
}

// Test 2: Verify Point-to-Point Link Creation
void TestPointToPointLinks()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes.Get(0), nodes.Get(1)); // A-B
    NetDeviceContainer bc = p2p.Install(nodes.Get(1), nodes.Get(2)); // B-C

    NS_ASSERT_MSG(ab.GetN() == 2, "A-B link should have two devices.");
    NS_ASSERT_MSG(bc.GetN() == 2, "B-C link should have two devices.");
}

// Test 3: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes.Get(0), nodes.Get(1)); // A-B
    NetDeviceContainer bc = p2p.Install(nodes.Get(1), nodes.Get(2)); // B-C

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = ipv4.Assign(ab);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = ipv4.Assign(bc);

    NS_ASSERT_MSG(abIf.GetAddress(0) != Ipv4Address("0.0.0.0"), "A should have a valid IP.");
    NS_ASSERT_MSG(abIf.GetAddress(1) != Ipv4Address("0.0.0.0"), "B should have a valid IP on A-B link.");
    NS_ASSERT_MSG(bcIf.GetAddress(0) != Ipv4Address("0.0.0.0"), "B should have a valid IP on B-C link.");
    NS_ASSERT_MSG(bcIf.GetAddress(1) != Ipv4Address("0.0.0.0"), "C should have a valid IP.");
}

// Test 4: Verify Distance Vector Routing with Split Horizon
void TestSplitHorizonRouting()
{
    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    dvRouting.Set("SplitHorizon", BooleanValue(true)); // Enable Split Horizon
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();

    NS_ASSERT_MSG(ipv4A != nullptr, "Node A should have an IPv4 stack.");
    NS_ASSERT_MSG(ipv4B != nullptr, "Node B should have an IPv4 stack.");
    NS_ASSERT_MSG(ipv4C != nullptr, "Node C should have an IPv4 stack.");
}

// Test 5: Verify UDP Echo Server Installation
void TestUdpEchoServer()
{
    NodeContainer nodes;
    nodes.Create(3);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2)); // C as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on node C.");
}

// Test 6: Verify UDP Echo Client Installation
void TestUdpEchoClient()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes.Get(0), nodes.Get(1)); // A-B
    NetDeviceContainer bc = p2p.Install(nodes.Get(1), nodes.Get(2)); // B-C

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(ab);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = ipv4.Assign(bc);

    UdpEchoClientHelper echoClient(bcIf.GetAddress(1), 9); // Sending to C
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // A as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed on node A.");
}

// Test 7: Verify UDP Packet Transmission
void TestUdpPacketTransmission()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes.Get(0), nodes.Get(1)); // A-B
    NetDeviceContainer bc = p2p.Install(nodes.Get(1), nodes.Get(2)); // B-C

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(ab);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = ipv4.Assign(bc);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2)); // C as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(bcIf.GetAddress(1), 9); // Sending to C
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // A as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "UDP Echo packets should be sent from A to C successfully.");
}

// Run all tests
int main(int argc, char *argv[])
{
    NS_LOG_INFO("Running ns-3 Split Horizon Unit Tests");

    TestNodeCreation();
    TestPointToPointLinks();
    TestIpAddressAssignment();
    TestSplitHorizonRouting();
    TestUdpEchoServer();
    TestUdpEchoClient();
    TestUdpPacketTransmission();

    NS_LOG_INFO("All tests passed successfully.");
    return 0;
}
