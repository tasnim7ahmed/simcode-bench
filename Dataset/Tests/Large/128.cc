void TestNodeCreation()
{
    NodeContainer routers;
    routers.Create(4);

    NS_ASSERT_MSG(routers.GetN() == 4, "Number of routers should be 4");
}

void TestPointToPointLinks()
{
    NodeContainer routers;
    routers.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer link1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer link2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer link3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer link4 = p2p.Install(routers.Get(0), routers.Get(3));

    NS_ASSERT_MSG(link1.GetN() == 2, "Link 1 should have exactly 2 devices");
    NS_ASSERT_MSG(link2.GetN() == 2, "Link 2 should have exactly 2 devices");
    NS_ASSERT_MSG(link3.GetN() == 2, "Link 3 should have exactly 2 devices");
    NS_ASSERT_MSG(link4.GetN() == 2, "Link 4 should have exactly 2 devices");
}

void TestIpAddressAssignment()
{
    NodeContainer routers;
    routers.Create(4);

    PointToPointHelper p2p;
    NetDeviceContainer link1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer link2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer link3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer link4 = p2p.Install(routers.Get(0), routers.Get(3));

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = ipv4.Assign(link1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = ipv4.Assign(link2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3 = ipv4.Assign(link3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i4 = ipv4.Assign(link4);

    NS_ASSERT_MSG(i1.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 1 should have a valid IP");
    NS_ASSERT_MSG(i2.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 2 should have a valid IP");
    NS_ASSERT_MSG(i3.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 3 should have a valid IP");
    NS_ASSERT_MSG(i4.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 4 should have a valid IP");
}

void TestOspfRouting()
{
    NodeContainer routers;
    routers.Create(4);

    OspfHelper ospfRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ospfRouting);
    internet.Install(routers);

    Ptr<Ipv4RoutingProtocol> ospfProtocol = routers.Get(0)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ospfProtocol != nullptr, "OSPF routing should be enabled on router 0");

    ospfProtocol = routers.Get(1)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ospfProtocol != nullptr, "OSPF routing should be enabled on router 1");

    ospfProtocol = routers.Get(2)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ospfProtocol != nullptr, "OSPF routing should be enabled on router 2");

    ospfProtocol = routers.Get(3)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ospfProtocol != nullptr, "OSPF routing should be enabled on router 3");
}
void TestUdpApplications()
{
    NodeContainer routers;
    routers.Create(4);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on Router 3");

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.4.2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed on Router 0");
}

void TestPacketTracing()
{
    NodeContainer routers;
    routers.Create(4);

    PointToPointHelper p2p;
    NetDeviceContainer link1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer link2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer link3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer link4 = p2p.Install(routers.Get(0), routers.Get(3));

    p2p.EnablePcapAll("lsa-ospf");

    // Check if tracing is enabled
    NS_ASSERT_MSG(system("ls lsa-ospf-0-0.pcap") == 0, "PCAP trace file should be generated");
}

void TestSimulationExecution()
{
    NodeContainer routers;
    routers.Create(4);

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    NS_ASSERT_MSG(Simulator::Now().GetSeconds() >= 12.0, "Simulation should run for at least 12 seconds");

    Simulator::Destroy();
}

void TestSimulationCleanUp()
{
    NodeContainer routers;
    routers.Create(4);

    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(Simulator::GetRemainingTime() == Time(0), "Simulation should be properly cleaned up");
}


