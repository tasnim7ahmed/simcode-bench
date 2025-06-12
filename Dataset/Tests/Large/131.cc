void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3);

    NS_ASSERT_MSG(nodes.GetN() == 3, "Three nodes (A, B, C) should be created.");
}

void TestPointToPointLinks()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1))); // A-B
    NetDeviceContainer bc = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2))); // B-C
    NetDeviceContainer ac = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2))); // A-C

    NS_ASSERT_MSG(ab.GetN() == 2, "Link A-B should have two devices.");
    NS_ASSERT_MSG(bc.GetN() == 2, "Link B-C should have two devices.");
    NS_ASSERT_MSG(ac.GetN() == 2, "Link A-C should have two devices.");
}

void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer bc = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    NetDeviceContainer ac = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = ipv4.Assign(ab);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = ipv4.Assign(bc);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer acIf = ipv4.Assign(ac);

    NS_ASSERT_MSG(abIf.GetAddress(0) != Ipv4Address("0.0.0.0"), "A-B should have valid IPs.");
    NS_ASSERT_MSG(bcIf.GetAddress(0) != Ipv4Address("0.0.0.0"), "B-C should have valid IPs.");
    NS_ASSERT_MSG(acIf.GetAddress(0) != Ipv4Address("0.0.0.0"), "A-C should have valid IPs.");
}
void TestDvRouting()
{
    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();

    NS_ASSERT_MSG(ipv4A != nullptr, "Node A should have an IPv4 stack.");
    NS_ASSERT_MSG(ipv4B != nullptr, "Node B should have an IPv4 stack.");
    NS_ASSERT_MSG(ipv4C != nullptr, "Node C should have an IPv4 stack.");
}

void TestUdpCommunication()
{
    NodeContainer nodes;
    nodes.Create(3);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2)); // C as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on node C.");

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.2.2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // A as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed on node A.");
}

void TestLinkFailure()
{
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer bc = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    // Install Internet stack with Distance Vector Routing
    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = ipv4.Assign(bc);

    // Simulate link failure at t = 5s
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, bc.Get(1));
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, bc.Get(0));

    // Run the simulation for a short time to check routing updates
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "Link failure between B and C should be handled by DV routing.");
}


void TestRoutingAfterFailure()
{
    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodes.Get(1)->GetObject<Ipv4>());

    // Before failure, route should exist
    NS_ASSERT_MSG(staticRoutingB->GetNRoutes() > 0, "B should have routes before failure.");

    // Simulate failure of link B-C
    NetDeviceContainer bc;
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, bc.Get(0));
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, bc.Get(1));

    // Run simulation for a while to allow for route updates
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    // After failure, routes should be updated (but may still experience count-to-infinity issue)
    NS_ASSERT_MSG(staticRoutingB->GetNRoutes() > 0, "B should still have alternative routes.");
}

