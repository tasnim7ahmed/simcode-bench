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

void TestNetworkDeviceInstallation()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1); // MTU 5000
    NetDeviceContainer d2 = csma.Install(net2); // Default MTU

    // Verify devices installation
    NS_TEST_ASSERT_MSG_EQ(d1.GetN(), 2, "Failed to install devices on net1!");
    NS_TEST_ASSERT_MSG_EQ(d2.GetN(), 2, "Failed to install devices on net2!");
}

void TestIpv6AddressAssignment()
{
    // Create nodes and networks
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1); // MTU 5000
    NetDeviceContainer d2 = csma.Install(net2); // Default MTU

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

    // Verify IPv6 assignment
    NS_TEST_ASSERT_MSG_EQ(i1.GetN(), 2, "IPv6 address assignment failed for net1!");
    NS_TEST_ASSERT_MSG_EQ(i2.GetN(), 2, "IPv6 address assignment failed for net2!");
}

void TestUdpEchoApplications()
{
    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1); // MTU 5000
    NetDeviceContainer d2 = csma.Install(net2); // Default MTU

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    // Create and install UDP Echo Client and Server
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

    // Verify application setup
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "UdpEchoClient application not installed on node n0!");
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "UdpEchoServer application not installed on node n1!");
}

void TestTraceFileGeneration()
{
    // Create nodes and networks
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1); // MTU 5000
    NetDeviceContainer d2 = csma.Install(net2); // Default MTU

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);

    // Enable trace
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Verify trace file generation
    std::ifstream traceFile("fragmentation-ipv6-two-mtu.tr");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "Trace file was not generated!");
}

void TestSimulationRun()
{
    // Run the simulation
    Simulator::Run();

    // Verify that the simulation runs without errors
    NS_TEST_ASSERT_MSG_EQ(Simulator::GetSimulationState(), Simulator::SIMULATION_READY, "Simulation did not run correctly!");

    // Destroy the simulation environment
    Simulator::Destroy();
}


