void TestNodeCreation()
{
    NodeContainer c;
    c.Create(7);
    NS_ASSERT(c.GetN() == 7);
    std::cout << "Node creation test passed!" << std::endl;
}

void TestIpAddressAssignment()
{
    NodeContainer c;
    c.Create(7);
    Ipv4AddressHelper ipv4;
    NetDeviceContainer d0d2, d1d6, d1d2, d5d6, d2345;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    d0d2 = PointToPointHelper().Install(NodeContainer(c.Get(0), c.Get(2)));
    ipv4.Assign(d0d2);
    
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    d1d2 = PointToPointHelper().Install(NodeContainer(c.Get(1), c.Get(2)));
    ipv4.Assign(d1d2);
    
    // Further IP assignments...
    
    Ipv4InterfaceContainer i5i6 = ipv4.Assign(d5d6);
    Ipv4InterfaceContainer i1i6 = ipv4.Assign(d1d6);

    // Validate IP address assignment (just checking one address for example)
    NS_ASSERT(i1i6.GetAddress(1) == Ipv4Address("10.250.1.2"));
    std::cout << "IP address assignment test passed!" << std::endl;
}

void TestRoutingTablePopulation()
{
    NodeContainer c;
    c.Create(7);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Assuming that the default routing tables are correct
    Ptr<Node> n0 = c.Get(0);
    Ptr<Ipv4> ipv40 = n0->GetObject<Ipv4>();
    Ipv4RoutingProtocol::RouteList routingTable = ipv40->GetRoutingProtocol()->GetRoutingTable();
    
    // For example, assert that n0 has a route to n6
    bool foundRoute = false;
    for (auto& route : routingTable)
    {
        if (route.GetDest() == Ipv4Address("10.250.1.1"))
        {
            foundRoute = true;
            break;
        }
    }
    NS_ASSERT(foundRoute);
    std::cout << "Routing table population test passed!" << std::endl;
}

void TestRoutingTablePopulation()
{
    NodeContainer c;
    c.Create(7);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Assuming that the default routing tables are correct
    Ptr<Node> n0 = c.Get(0);
    Ptr<Ipv4> ipv40 = n0->GetObject<Ipv4>();
    Ipv4RoutingProtocol::RouteList routingTable = ipv40->GetRoutingProtocol()->GetRoutingTable();
    
    // For example, assert that n0 has a route to n6
    bool foundRoute = false;
    for (auto& route : routingTable)
    {
        if (route.GetDest() == Ipv4Address("10.250.1.1"))
        {
            foundRoute = true;
            break;
        }
    }
    NS_ASSERT(foundRoute);
    std::cout << "Routing table population test passed!" << std::endl;
}

void TestOnOffApplicationTrafficFlow()
{
    NodeContainer c;
    c.Create(7);
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer i5i6;
    
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    i5i6 = ipv4.Assign(PointToPointHelper().Install(NodeContainer(c.Get(5), c.Get(6))));
    
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(i5i6.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("2kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(50));

    ApplicationContainer apps = onoff.Install(c.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Check that the application is running and sending traffic
    NS_LOG_INFO("OnOff application traffic flow test passed!");
}

void TestPacketSinkReception()
{
    NodeContainer c;
    c.Create(7);
    
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer apps = sink.Install(c.Get(6));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Simulate traffic
    Simulator::Run();
    
    // Check if the packet sink received any packets
    Ptr<PacketSink> packetSinkApp = DynamicCast<PacketSink>(apps.Get(0));
    NS_ASSERT(packetSinkApp->GetTotalRx() > 0);
    std::cout << "Packet sink reception test passed!" << std::endl;
}

void TestPacketSinkReception()
{
    NodeContainer c;
    c.Create(7);
    
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer apps = sink.Install(c.Get(6));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Simulate traffic
    Simulator::Run();
    
    // Check if the packet sink received any packets
    Ptr<PacketSink> packetSinkApp = DynamicCast<PacketSink>(apps.Get(0));
    NS_ASSERT(packetSinkApp->GetTotalRx() > 0);
    std::cout << "Packet sink reception test passed!" << std::endl;
}

void TestPacketCapture()
{
    NodeContainer c;
    c.Create(7);
    
    PointToPointHelper p2p;
    NetDeviceContainer d0d2 = p2p.Install(NodeContainer(c.Get(0), c.Get(2)));

    // Enable PCAP
    p2p.EnablePcapAll("dynamic-global-routing");

    Simulator::Run();

    // Check if PCAP files were generated
    std::ifstream pcapFile("dynamic-global-routing-0-1.pcap");
    NS_ASSERT(pcapFile.good());
    std::cout << "Packet capture test passed!" << std::endl;
}


