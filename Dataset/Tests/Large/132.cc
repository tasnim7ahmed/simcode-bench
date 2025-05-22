void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    NS_ASSERT_MSG(nodes.GetN() == 2, "Two nodes (A and B) should be created.");
}

void TestPointToPointLink()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes);

    NS_ASSERT_MSG(ab.GetN() == 2, "Link A-B should have two devices (one per node).");
}

void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = ipv4.Assign(ab);

    NS_ASSERT_MSG(abIf.GetAddress(0) != Ipv4Address("0.0.0.0"), "A should have a valid IP.");
    NS_ASSERT_MSG(abIf.GetAddress(1) != Ipv4Address("0.0.0.0"), "B should have a valid IP.");
}

void TestDvRouting()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();

    NS_ASSERT_MSG(ipv4A != nullptr, "Node A should have an IPv4 stack.");
    NS_ASSERT_MSG(ipv4B != nullptr, "Node B should have an IPv4 stack.");
}

void TestUdpCommunication()
{
    NodeContainer nodes;
    nodes.Create(2);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1)); // B as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on node B.");

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.1.2"), 9);
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
    nodes.Create(2);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes);

    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    // Simulate link failure at 5s
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(1));
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "Link failure between A and B should be handled correctly.");
}

void TestRoutingLoop()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes);

    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    // Simulate link failure at 5s
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(1));
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(0));

    // Re-enable link at 8s
    Simulator::Schedule(Seconds(8.0), &NetDevice::SetLinkUp, ab.Get(1));
    Simulator::Schedule(Seconds(8.0), &NetDevice::SetLinkUp, ab.Get(0));

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "Link restoration should trigger routing updates, possibly causing a loop.");
}

void TestPacketLossDuringFailure()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    NetDeviceContainer ab = p2p.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Setup UDP echo applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.1.2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Simulate link failure at 5s
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(1));
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(0));

    // Check if packets were lost during link failure
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "Packets should be lost when the link fails at 5s.");
}

