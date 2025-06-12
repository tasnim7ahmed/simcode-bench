void TestNodeCreation()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    // Verify node creation
    NS_TEST_ASSERT_MSG_NE(n0, nullptr, "Node n0 creation failed!");
    NS_TEST_ASSERT_MSG_NE(r1, nullptr, "Node r1 creation failed!");
    NS_TEST_ASSERT_MSG_NE(n1, nullptr, "Node n1 creation failed!");
    NS_TEST_ASSERT_MSG_NE(r2, nullptr, "Node r2 creation failed!");
    NS_TEST_ASSERT_MSG_NE(n2, nullptr, "Node n2 creation failed!");
}

void TestNetworkDevicesSetup()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    NodeContainer net1(n0, r1);
    NodeContainer net2(r1, n1, r2);
    NodeContainer net3(r2, n2);

    // Create channels and devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(2000));
    NetDeviceContainer d2 = csma.Install(net2); // CSMA Network with MTU 2000

    csma.SetDeviceAttribute("Mtu", UintegerValue(5000));
    NetDeviceContainer d1 = csma.Install(net1); // CSMA Network with MTU 5000

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(5000000));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer d3 = pointToPoint.Install(net3); // P2P Network with MTU 1500

    // Verify device installation
    NS_TEST_ASSERT_MSG_EQ(d1.GetN(), 2, "Failed to install devices on net1!");
    NS_TEST_ASSERT_MSG_EQ(d2.GetN(), 3, "Failed to install devices on net2!");
    NS_TEST_ASSERT_MSG_EQ(d3.GetN(), 2, "Failed to install devices on net3!");
}

void TestIpAddressAssignment()
{
    // Create nodes and network devices
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    NodeContainer net1(n0, r1);
    NodeContainer net2(r1, n1, r2);
    NodeContainer net3(r2, n2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(2000));
    NetDeviceContainer d2 = csma.Install(net2);
    
    csma.SetDeviceAttribute("Mtu", UintegerValue(5000));
    NetDeviceContainer d1 = csma.Install(net1);
    
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(5000000));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer d3 = pointToPoint.Install(net3);

    Ipv6AddressHelper ipv6;
    
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i3 = ipv6.Assign(d3);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_EQ(i1.GetN(), 2, "Failed to assign IPv6 addresses to net1!");
    NS_TEST_ASSERT_MSG_EQ(i2.GetN(), 3, "Failed to assign IPv6 addresses to net2!");
    NS_TEST_ASSERT_MSG_EQ(i3.GetN(), 2, "Failed to assign IPv6 addresses to net3!");
}

void TestRoutingTable()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    NodeContainer all(n0, r1, n1, r2, n2);

    // Install IPv6 stack
    InternetStackHelper internetv6;
    internetv6.Install(all);

    // Create channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(NodeContainer(n0, r1));
    NetDeviceContainer d2 = csma.Install(NodeContainer(r1, n1, r2));

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    // Create a stream for routing table output
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);

    // Print routing table at time 0 for router `r1`
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(0), r1, routingStream);

    // This can be expanded by verifying specific entries in the routing table
}

void TestApplicationSetup()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    NodeContainer net1(n0, r1);
    NodeContainer net2(r1, n1, r2);
    NodeContainer net3(r2, n2);

    // Create applications (UdpEchoClient and UdpEchoServer)
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i3 = ipv6.Assign(csma.Install(net3));

    uint32_t maxPacketCount = 5;
    uint32_t packetSizeN1 = 1600;

    UdpEchoClientHelper echoClient(i3.GetAddress(1, 1), 42);
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSizeN1));
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ApplicationContainer clientApps = echoClient.Install(n1);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer(42);
    ApplicationContainer serverApps = echoServer.Install(n2);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(30.0));

    // Verify application installation
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "UdpEchoClient application not installed on node n1!");
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "UdpEchoServer application not installed on node n2!");
}

void TestTraceGeneration()
{
    // Create nodes and devices
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    NodeContainer net1(n0, r1);
    NodeContainer net2(r1, n1, r2);
    NodeContainer net3(r2, n2);

    // Create channels and devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(2000));
    NetDeviceContainer d2 = csma.Install(net2);

    csma.SetDeviceAttribute("Mtu", UintegerValue(5000));
    NetDeviceContainer d1 = csma.Install(net1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(5000000));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer d3 = pointToPoint.Install(net3);

    // Enable trace and pcap
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    csma.EnablePcapAll("fragmentation-ipv6-PMTU", true);
    pointToPoint.EnablePcapAll("fragmentation-ipv6-PMTU", true);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Verify trace file generation
    std::ifstream traceFile("fragmentation-ipv6-PMTU.tr");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "Trace file not generated!");

    // Optionally, check the pcap file generation as well
    std::ifstream pcapFile("fragmentation-ipv6-PMTU-0-0.pcap");
    NS_TEST_ASSERT_MSG_EQ(pcapFile.is_open(), true, "Pcap file not generated!");
}

