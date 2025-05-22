void TestNodeCreation()
{
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nB = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();
    NodeContainer c = NodeContainer(nA, nB, nC);
    NS_ASSERT(c.GetN() == 3);  // Ensure 3 nodes are created
    std::cout << "Node creation test passed!" << std::endl;
}

void TestIpAddressAssignment()
{
    NodeContainer c;
    c.Create(3);
    Ipv4AddressHelper ipv4;
    
    // Assign IP addresses
    ipv4.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iAiB = ipv4.Assign(PointToPointHelper().Install(NodeContainer(c.Get(0), c.Get(1))));
    ipv4.SetBase("10.1.1.4", "255.255.255.252");
    Ipv4InterfaceContainer iBiC = ipv4.Assign(PointToPointHelper().Install(NodeContainer(c.Get(1), c.Get(2))));

    // Check the IP address assignment
    NS_ASSERT(iAiB.GetAddress(1) == Ipv4Address("10.1.1.2"));
    NS_ASSERT(iBiC.GetAddress(1) == Ipv4Address("10.1.1.6"));
    
    std::cout << "IP address assignment test passed!" << std::endl;
}

void TestRoutingTablePopulation()
{
    NodeContainer c;
    c.Create(3);

    // Set up routing and populate tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Check the routing table for node nA (node 0)
    Ptr<Node> nA = c.Get(0);
    Ptr<Ipv4> ipv4A = nA->GetObject<Ipv4>();
    Ipv4RoutingProtocol::RouteList routingTable = ipv4A->GetRoutingProtocol()->GetRoutingTable();
    
    // Ensure there's a route to node nC (node 2)
    bool foundRoute = false;
    for (auto& route : routingTable)
    {
        if (route.GetDest() == Ipv4Address("192.168.1.1"))
        {
            foundRoute = true;
            break;
        }
    }
    NS_ASSERT(foundRoute);
    std::cout << "Routing table population test passed!" << std::endl;
}

void TestStaticRouteInjection()
{
    NodeContainer c;
    c.Create(3);
    
    // Set up static routing for Node B (node 1)
    Ptr<Node> nB = c.Get(1);
    Ptr<Ipv4> ipv4B = nB->GetObject<Ipv4>();
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingB = ipv4RoutingHelper.GetStaticRouting(ipv4B);
    
    // Add a static route to node C (192.168.1.1)
    staticRoutingB->AddHostRouteTo(Ipv4Address("192.168.1.1"), Ipv4Address("10.1.1.6"), 2);

    // Check if the static route is added
    Ipv4RoutingTableEntry route = staticRoutingB->GetRoute(0);  // Get the first route
    NS_ASSERT(route.GetDest() == Ipv4Address("192.168.1.1"));
    
    std::cout << "Static route injection test passed!" << std::endl;
}

void TestOnOffApplicationTrafficFlow()
{
    NodeContainer c;
    c.Create(3);

    uint16_t port = 9;  // Discard port
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iAiB = ipv4.Assign(PointToPointHelper().Install(NodeContainer(c.Get(0), c.Get(1))));
    
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(iAiB.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate(6000));
    ApplicationContainer apps = onoff.Install(c.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Simulate and check if traffic is sent
    Simulator::Run();
    
    std::cout << "OnOff application traffic flow test passed!" << std::endl;
}

void TestPacketSinkReception()
{
    NodeContainer c;
    c.Create(3);

    uint16_t port = 9;  // Discard port
    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer apps = sink.Install(c.Get(2));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    Ptr<PacketSink> packetSinkApp = DynamicCast<PacketSink>(apps.Get(0));
    NS_ASSERT(packetSinkApp->GetTotalRx() > 0);  // Ensure that some packets were received
    
    std::cout << "Packet sink reception test passed!" << std::endl;
}

void TestTraceFileGeneration()
{
    NodeContainer c;
    c.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dAdB = p2p.Install(NodeContainer(c.Get(0), c.Get(1)));
    NetDeviceContainer dBdC = p2p.Install(NodeContainer(c.Get(1), c.Get(2)));

    // Enable trace output
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("global-routing-test.tr"));
    p2p.EnablePcapAll("global-routing-test");

    // Run the simulation
    Simulator::Run();

    // Check if the trace file was created
    std::ifstream traceFile("global-routing-test.tr");
    NS_ASSERT(traceFile.good());
    std::cout << "Trace file generation test passed!" << std::endl;
}

void TestPacketCapture()
{
    NodeContainer c;
    c.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dAdB = p2p.Install(NodeContainer(c.Get(0), c.Get(1)));
    NetDeviceContainer dBdC = p2p.Install(NodeContainer(c.Get(1), c.Get(2)));

    // Enable PCAP capture
    p2p.EnablePcapAll("global-routing-test");

    // Run the simulation
    Simulator::Run();

    // Check if the PCAP file exists
    std::ifstream pcapFile("global-routing-test-0-1.pcap");
    NS_ASSERT(pcapFile.good());
    std::cout << "Packet capture test passed!" << std::endl;
}

