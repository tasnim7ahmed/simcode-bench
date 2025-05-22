void TestNodeCreation()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    // Verify node creation
    NS_TEST_ASSERT_MSG_NE(n0, nullptr, "Node n0 creation failed!");
    NS_TEST_ASSERT_MSG_NE(r, nullptr, "Node r creation failed!");
    NS_TEST_ASSERT_MSG_NE(n1, nullptr, "Node n1 creation failed!");
}

void TestNetworkAndDeviceSetup()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    // Create networks
    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Create CSMA channels and devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // Verify device installation
    NS_TEST_ASSERT_MSG_EQ(d1.GetN(), 2, "Failed to install devices on net1!");
    NS_TEST_ASSERT_MSG_EQ(d2.GetN(), 2, "Failed to install devices on net2!");
}

void TestIpv6AddressAssignment()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    // Create networks and devices
    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    i1.SetForwarding(1, true);
    i1.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);
    i2.SetForwarding(0, true);
    i2.SetDefaultRouteInAllNodes(0);

    // Check IPv6 addresses
    NS_TEST_ASSERT_MSG_EQ(i1.GetN(), 2, "IPv6 addresses not assigned to net1 correctly!");
    NS_TEST_ASSERT_MSG_EQ(i2.GetN(), 2, "IPv6 addresses not assigned to net2 correctly!");
}

void TestRoutingTableConfiguration()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer all(n0, r, n1);

    // Install Internet stack
    InternetStackHelper internetv6;
    internetv6.Install(all);

    // Set up network and devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(NodeContainer(n0, r));
    NetDeviceContainer d2 = csma.Install(NodeContainer(r, n1));

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    i1.SetForwarding(1, true);
    i1.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);
    i2.SetForwarding(0, true);
    i2.SetDefaultRouteInAllNodes(0);

    StackHelper stackHelper;
    stackHelper.PrintRoutingTable(n0);

    // This test is based on output, so you may want to redirect the output to a file
    // and verify manually, or capture the output and check for expected entries.
}

void TestPingApplication()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    // Create network and devices
    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    i1.SetForwarding(1, true);
    i1.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);
    i2.SetForwarding(0, true);
    i2.SetDefaultRouteInAllNodes(0);

    // Set up the ping application
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    PingHelper ping(i2.GetAddress(1, 1));
    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ApplicationContainer apps = ping.Install(net1.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(20.0));

    // Run the simulation
    Simulator::Run();

    // Check ping results in the logs
    // Normally, this test would check the number of packets sent and received
    // You can analyze the pcap or logs if necessary

    Simulator::Destroy();
}

void TestTraceGeneration()
{
    // Create nodes and devices
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    // Create network and devices
    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    i1.SetForwarding(1, true);
    i1.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);
    i2.SetForwarding(0, true);
    i2.SetDefaultRouteInAllNodes(0);

    // Enable trace and pcap
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("simple-routing-ping6.tr"));
    csma.EnablePcapAll("simple-routing-ping6", true);

    // Run the simulation
    Simulator::Run();

    // Verify if the trace file is generated
    std::ifstream traceFile("simple-routing-ping6.tr");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "Trace file not generated!");

    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestNetworkAndDeviceSetup();
    TestIpv6AddressAssignment();
    TestRoutingTableConfiguration();
    TestPingApplication();
    TestTraceGeneration();

    return 0;
}


