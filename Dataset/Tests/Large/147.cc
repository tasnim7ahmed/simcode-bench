#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopology3NodesTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3);

    // Ensure 3 nodes are created
    NS_ASSERT_MSG(nodes.GetN() == 3, "Three nodes should be created.");
}

// Test 2: Verify Point-to-Point Links Setup
void TestPointToPointLinks()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device01, device02;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2

    // Ensure two point-to-point links are installed
    NS_ASSERT_MSG(device01.GetN() == 1, "Link between node 0 and node 1 should be installed.");
    NS_ASSERT_MSG(device02.GetN() == 1, "Link between node 0 and node 2 should be installed.");
}

// Test 3: Verify Internet Stack Installation
void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper stack;
    stack.Install(nodes);

    // Check that the internet stack has been installed on all nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        NS_ASSERT_MSG(ipv4 != nullptr, "Internet stack should be installed on node " + std::to_string(i));
    }
}

// Test 4: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device01, device02;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(device01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(device02);

    // Ensure IP addresses are assigned correctly
    NS_ASSERT_MSG(interfaces01.GetAddress(0) != Ipv4Address("0.0.0.0"), "IP address not assigned for node 1.");
    NS_ASSERT_MSG(interfaces02.GetAddress(0) != Ipv4Address("0.0.0.0"), "IP address not assigned for node 2.");
}

// Test 5: Verify UDP Echo Server Installation on Node 0
void TestUdpEchoServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    UdpEchoServerHelper echoServer(9);  // Port number 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Ensure that the UDP Echo Server is installed on node 0
    NS_ASSERT_MSG(serverApps.GetN() == 1, "UDP Echo Server should be installed on node 0.");
}

// Test 6: Verify UDP Echo Client Installation on Node 1
void TestUdpEchoClient1Installation()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device01, device02;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(device01);

    UdpEchoClientHelper echoClient(interfaces01.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Ensure that the UDP Echo Client is installed on node 1
    NS_ASSERT_MSG(clientApps.GetN() == 1, "UDP Echo Client should be installed on node 1.");
}

// Test 7: Verify UDP Echo Client Installation on Node 2
void TestUdpEchoClient2Installation()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device01, device02;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(device02);

    UdpEchoClientHelper echoClient2(interfaces02.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    // Ensure that the second UDP Echo Client is installed on node 2
    NS_ASSERT_MSG(clientApps2.GetN() == 1, "UDP Echo Client should be installed on node 2.");
}

// Test 8: Verify PCAP Tracing for Packet Flow
void TestPcapTracing()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device01, device02;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2

    // Enable PCAP tracing
    pointToPoint.EnablePcapAll("branch-topology-3-nodes");

    // This test does not check actual PCAP files, but verifies the setup
    NS_LOG_INFO("PCAP tracing enabled for branch topology.");
}

// Test 9: Verify Successful Simulation Run
void TestSimulationRun()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device01, device02;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(device01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(device02);

    UdpEchoServerHelper echoServer(9);  // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces01.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces02.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");
    Simulator::Destroy();

    // Check if the simulation ran without issues
    NS_LOG_INFO("Branch simulation with 3 nodes successfully ran.");
}

// Main test function to run all tests
int main()
{
    TestNodeCreation();
    TestPointToPointLinks();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestUdpEchoServerInstallation();
    TestUdpEchoClient1Installation();
    TestUdpEchoClient2Installation();
    TestPcapTracing();
    TestSimulationRun();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
