
// Network topology
// //
// //             n0   r    n1
// //             |    _    |
// //             ====|_|====
// //                router
// //
// // - Tracing of queues and packet receptions to file "fragmentation-ipv6.tr"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6Example");

int
main(int argc, char** argv)
{
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
    }
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    NS_LOG_INFO("Create nodes.");
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);
    NodeContainer all(n0, r, n1);

    NS_LOG_INFO("Create IPv6 Internet Stack");
    InternetStackHelper internetv6;
    internetv6.Install(all);

    NS_LOG_INFO("Create channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    NS_LOG_INFO("Create networks and assign IPv6 Addresses.");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    i1.SetForwarding(1, true);
    i1.SetDefaultRouteInAllNodes(1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);
    i2.SetForwarding(0, true);
    i2.SetDefaultRouteInAllNodes(0);

    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(0), n0, routingStream);

    /* Create a UdpEchoClient and UdpEchoServer application to send packets from n0 to n1 via r */
    uint32_t packetSize = 4096;
    uint32_t maxPacketCount = 5;

    UdpEchoClientHelper echoClient(i2.GetAddress(1, 1), 42);
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ApplicationContainer clientApps = echoClient.Install(net1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    UdpEchoServerHelper echoServer(42);
    ApplicationContainer serverApps = echoServer.Install(net2.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(30.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6.tr"));
    csma.EnablePcapAll(std::string("fragmentation-ipv6"), true);

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}

void TestNodeCreation()
{
    // Create nodes
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nB = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();

    // Verify node creation
    NS_TEST_ASSERT_MSG_NE(nA, nullptr, "Node A creation failed!");
    NS_TEST_ASSERT_MSG_NE(nB, nullptr, "Node B creation failed!");
    NS_TEST_ASSERT_MSG_NE(nC, nullptr, "Node C creation failed!");
}

void TestNetworkDevicesSetup()
{
    // Create nodes
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nB = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();

    NodeContainer c = NodeContainer(nA, nB, nC);

    // Point-to-point links and devices
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dAdB = p2p.Install(NodeContainer(nA, nB));
    NetDeviceContainer dBdC = p2p.Install(NodeContainer(nB, nC));

    Ptr<CsmaNetDevice> deviceA = CreateObject<CsmaNetDevice>();
    deviceA->SetAddress(Mac48Address::Allocate());
    nA->AddDevice(deviceA);
    deviceA->SetQueue(CreateObject<DropTailQueue<Packet>>());

    Ptr<CsmaNetDevice> deviceC = CreateObject<CsmaNetDevice>();
    deviceC->SetAddress(Mac48Address::Allocate());
    nC->AddDevice(deviceC);
    deviceC->SetQueue(CreateObject<DropTailQueue<Packet>>());

    // Verify devices installation
    NS_TEST_ASSERT_MSG_EQ(dAdB.GetN(), 2, "Failed to install devices on nodes A and B!");
    NS_TEST_ASSERT_MSG_EQ(dBdC.GetN(), 2, "Failed to install devices on nodes B and C!");
    NS_TEST_ASSERT_MSG_NE(deviceA, nullptr, "Failed to create device for node A!");
    NS_TEST_ASSERT_MSG_NE(deviceC, nullptr, "Failed to create device for node C!");
}

void TestStaticRoutingSetup()
{
    // Create nodes
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nB = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();

    // Set up IPv4 routing
    Ptr<Ipv4> ipv4A = nA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nB->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nC->GetObject<Ipv4>();

    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    // Static routing for node A
    Ptr<Ipv4StaticRouting> staticRoutingA = ipv4RoutingHelper.GetStaticRouting(ipv4A);
    staticRoutingA->AddHostRouteTo(Ipv4Address("192.168.1.1"), Ipv4Address("10.1.1.2"), 1);

    // Static routing for node B
    Ptr<Ipv4StaticRouting> staticRoutingB = ipv4RoutingHelper.GetStaticRouting(ipv4B);
    staticRoutingB->AddHostRouteTo(Ipv4Address("192.168.1.1"), Ipv4Address("10.1.1.6"), 2);

    // Verify static routes
    NS_TEST_ASSERT_MSG_EQ(staticRoutingA->GetNRoutes(), 1, "Static route not added on node A!");
    NS_TEST_ASSERT_MSG_EQ(staticRoutingB->GetNRoutes(), 1, "Static route not added on node B!");
}

void TestApplicationSetup()
{
    // Create nodes
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();

    // Set up applications
    Ipv4Address ifInAddrC("192.168.1.1");
    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(ifInAddrC, port)));
    onoff.SetConstantRate(DataRate(6000));
    ApplicationContainer apps = onoff.Install(nA);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(nC);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Verify applications installation
    NS_TEST_ASSERT_MSG_EQ(apps.GetN(), 2, "Failed to install applications on nodes A and C!");
}

void TestTraceGeneration()
{
    // Create nodes and devices
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nB = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();

    // Point-to-point devices and IP assignment
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dAdB = p2p.Install(NodeContainer(nA, nB));
    NetDeviceContainer dBdC = p2p.Install(NodeContainer(nB, nC));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iAiB = ipv4.Assign(dAdB);

    ipv4.SetBase("10.1.1.4", "255.255.255.252");
    Ipv4InterfaceContainer iBiC = ipv4.Assign(dBdC);

    // Enable trace and pcap
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("static-routing-slash32.tr"));
    p2p.EnablePcapAll("static-routing-slash32");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    // Verify trace file generation
    std::ifstream traceFile("static-routing-slash32.tr");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "Trace file not generated!");
}

int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestNetworkDevicesSetup();
    TestIpAddressAssignment();
    TestStaticRoutingSetup();
    TestApplicationSetup();
    TestTraceGeneration();

    return 0;
}

