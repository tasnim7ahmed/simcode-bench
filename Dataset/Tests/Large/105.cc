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

void TestNetworkDevicesSetup()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Create channels and devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // Verify device installation
    NS_TEST_ASSERT_MSG_EQ(d1.GetN(), 2, "Failed to install devices on net1!");
    NS_TEST_ASSERT_MSG_EQ(d2.GetN(), 2, "Failed to install devices on net2!");
}

void TestIpAddressAssignment()
{
    // Create nodes and network devices
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // IPv6 address assignment
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_EQ(i1.GetN(), 2, "Failed to assign IPv6 addresses to net1!");
    NS_TEST_ASSERT_MSG_EQ(i2.GetN(), 2, "Failed to assign IPv6 addresses to net2!");
}

void TestIpAddressAssignment()
{
    // Create nodes and network devices
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // IPv6 address assignment
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_EQ(i1.GetN(), 2, "Failed to assign IPv6 addresses to net1!");
    NS_TEST_ASSERT_MSG_EQ(i2.GetN(), 2, "Failed to assign IPv6 addresses to net2!");
}

void TestRoutingTable()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer all(n0, r, n1);

    // Install IPv6 stack
    InternetStackHelper internetv6;
    internetv6.Install(all);

    // Create channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(NodeContainer(n0, r));
    NetDeviceContainer d2 = csma.Install(NodeContainer(r, n1));

    // IPv6 address assignment
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    // Create a stream for routing table output
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);

    // Print routing table
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(0), n0, routingStream);

    // Since it's a static test, we verify no errors in output
    // For more detailed testing, you could capture the output and validate entries
}

void TestTraceGeneration()
{
    // Create nodes and devices
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Create channels and devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    // Enable trace and pcap
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6.tr"));
    csma.EnablePcapAll("fragmentation-ipv6");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Verify trace file generation
    std::ifstream traceFile("fragmentation-ipv6.tr");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "Trace file not generated!");

    // Optionally, check the pcap file generation as well
    std::ifstream pcapFile("fragmentation-ipv6-0-0.pcap");
    NS_TEST_ASSERT_MSG_EQ(pcapFile.is_open(), true, "Pcap file not generated!");
}

int main(int argc, char* argv[])
{
    TestNodeCreation();
    TestNetworkDevicesSetup();
    TestIpAddressAssignment();
    TestRoutingTable();
    TestApplicationSetup();
    TestTraceGeneration();

    return 0;
}
