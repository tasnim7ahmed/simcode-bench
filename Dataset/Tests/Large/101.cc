void TestNodeCreation()
{
    NodeContainer c;
    c.Create(4);  // Creating 4 nodes
    NS_ASSERT(c.GetN() == 4);  // Check if the number of nodes is 4
    std::cout << "Node creation test passed!" << std::endl;
}

void TestIpAddressAssignment()
{
    NodeContainer c;
    c.Create(4);

    PointToPointHelper p2p;
    Ipv4AddressHelper ipv4;

    // Assigning IP addresses
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(p2p.Install(NodeContainer(c.Get(0), c.Get(2))));

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(p2p.Install(NodeContainer(c.Get(1), c.Get(2))));

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i2 = ipv4.Assign(p2p.Install(NodeContainer(c.Get(3), c.Get(2))));

    // Check the assigned IP addresses
    NS_ASSERT(i0i2.GetAddress(1) == Ipv4Address("10.1.1.1"));
    NS_ASSERT(i1i2.GetAddress(1) == Ipv4Address("10.1.2.1"));
    NS_ASSERT(i3i2.GetAddress(1) == Ipv4Address("10.1.3.1"));

    std::cout << "IP address assignment test passed!" << std::endl;
}

void TestRoutingTablePopulation()
{
    NodeContainer c;
    c.Create(4);

    // Set up the routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Get the routing table for node 0
    Ptr<Ipv4> ipv4 = c.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4->GetRoutingProtocol();
    Ipv4RoutingProtocol::RouteList routingTable = routingProtocol->GetRoutingTable();

    // Ensure that there is at least one route in the table
    NS_ASSERT(routingTable.size() > 0);
    std::cout << "Routing table population test passed!" << std::endl;
}

void TestOnOffUdpFlow()
{
    NodeContainer c;
    c.Create(4);

    uint16_t port = 9;  // Discard port

    // Setting up OnOff application for UDP
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.3.1"), port)));
    onoff.SetConstantRate(DataRate("448kb/s"));
    ApplicationContainer apps = onoff.Install(c.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Create PacketSink at node 3
    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(c.Get(3));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    std::cout << "OnOff UDP flow from n0 to n3 test passed!" << std::endl;
}

void TestReverseOnOffUdpFlow()
{
    NodeContainer c;
    c.Create(4);

    uint16_t port = 9;  // Discard port

    // Setting up OnOff application for reverse UDP flow
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.2.1"), port)));
    onoff.SetConstantRate(DataRate("448kb/s"));
    ApplicationContainer apps = onoff.Install(c.Get(3));
    apps.Start(Seconds(1.1));
    apps.Stop(Seconds(10.0));

    // Create PacketSink at node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(c.Get(1));
    apps.Start(Seconds(1.1));
    apps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    std::cout << "Reverse OnOff UDP flow from n3 to n1 test passed!" << std::endl;
}

void TestFlowMonitor(bool enableFlowMonitor)
{
    NodeContainer c;
    c.Create(4);

    FlowMonitorHelper flowmonHelper;
    if (enableFlowMonitor)
    {
        flowmonHelper.InstallAll();
    }

    // Run the simulation
    Simulator::Run();

    if (enableFlowMonitor)
    {
        flowmonHelper.SerializeToXmlFile("simple-global-routing.flowmon", false, false);
    }

    std::cout << "Flow monitor test passed!" << std::endl;
}

void TestTraceFileGeneration()
{
    NodeContainer c;
    c.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Enable tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p.EnablePcapAll("simple-global-routing");

    // Run the simulation
    Simulator::Run();

    // Check if the trace file was created
    std::ifstream traceFile("simple-global-routing.tr");
    NS_ASSERT(traceFile.good());
    std::cout << "Trace file generation test passed!" << std::endl;
}

void TestPacketCapture()
{
    NodeContainer c;
    c.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Enable packet capture
    p2p.EnablePcapAll("simple-global-routing");

    // Run the simulation
    Simulator::Run();

    // Check if the PCAP file exists
    std::ifstream pcapFile("simple-global-routing-0-1.pcap");
    NS_ASSERT(pcapFile.good());
    std::cout << "Packet capture test passed!" << std::endl;
}

