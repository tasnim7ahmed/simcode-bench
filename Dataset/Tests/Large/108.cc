void TestNodeCreation()
{
    // Create nodes
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    // Verify node creation
    NS_TEST_ASSERT_MSG_NE(sta1, nullptr, "Node STA1 creation failed!");
    NS_TEST_ASSERT_MSG_NE(r1, nullptr, "Node R1 creation failed!");
    NS_TEST_ASSERT_MSG_NE(r2, nullptr, "Node R2 creation failed!");
    NS_TEST_ASSERT_MSG_NE(sta2, nullptr, "Node STA2 creation failed!");
}

void TestNetworkDeviceInstallation()
{
    // Create nodes
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    NodeContainer net1(sta1, r1, r2);
    NodeContainer net2(r2, sta2);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    // Verify devices installation
    NS_TEST_ASSERT_MSG_EQ(ndc1.GetN(), 3, "Failed to install devices on net1!");
    NS_TEST_ASSERT_MSG_EQ(ndc2.GetN(), 2, "Failed to install devices on net2!");
}

void TestIpv6AddressAssignment()
{
    // Create nodes and networks
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    NodeContainer net1(sta1, r1, r2);
    NodeContainer net2(r2, sta2);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic2 = ipv6.Assign(ndc2);

    // Verify IPv6 assignment
    NS_TEST_ASSERT_MSG_EQ(iic1.GetN(), 3, "IPv6 address assignment failed for net1!");
    NS_TEST_ASSERT_MSG_EQ(iic2.GetN(), 2, "IPv6 address assignment failed for net2!");
}

void TestStaticRouting()
{
    // Create nodes and networks
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    NodeContainer net1(sta1, r1, r2);
    NodeContainer net2(r2, sta2);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic2 = ipv6.Assign(ndc2);

    // Set static route on Router R1
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> routing = routingHelper.GetStaticRouting(r1->GetObject<Ipv6>());
    routing->AddHostRouteTo(iic2.GetAddress(1, 1), iic1.GetAddress(2, 0), iic1.GetInterfaceIndex(1));

    // Verify static route setup
    Ptr<Ipv6RoutingTableEntry> route = routing->Lookup(Ipv6Address("2001:2::1"));
    NS_TEST_ASSERT_MSG_NE(route, nullptr, "Static route setup failed!");
}

void TestPingApplication()
{
    // Create nodes and networks
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    NodeContainer net1(sta1, r1, r2);
    NodeContainer net2(r2, sta2);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic2 = ipv6.Assign(ndc2);

    // Create and install Ping application
    PingHelper ping(iic2.GetAddress(1, 1));
    ping.SetAttribute("Count", UintegerValue(5));
    ping.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer apps = ping.Install(sta1);
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // Verify application installation
    NS_TEST_ASSERT_MSG_EQ(apps.GetN(), 1, "Ping application not installed on STA1!");
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

void TestTraceFileGeneration()
{
    // Create nodes and networks
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    NodeContainer net1(sta1, r1, r2);
    NodeContainer net2(r2, sta2);

    // Install Csma devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic2 = ipv6.Assign(ndc2);

    // Enable trace
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("icmpv6-redirect.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Verify trace file generation
    std::ifstream traceFile("icmpv6-redirect.tr");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "Trace file was not generated!");
}

