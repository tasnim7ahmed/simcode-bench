void TestNodeCreation()
{
    NodeContainer routers;
    routers.Create(3);

    NS_ASSERT_MSG(routers.GetN() == 3, "Number of routers should be 3");
}

void TestPointToPointLinks()
{
    NodeContainer routers;
    routers.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerLink1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer routerLink2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer routerLink3 = p2p.Install(routers.Get(0), routers.Get(2));

    NS_ASSERT_MSG(routerLink1.GetN() == 2, "Router link 1 should have exactly 2 devices");
    NS_ASSERT_MSG(routerLink2.GetN() == 2, "Router link 2 should have exactly 2 devices");
    NS_ASSERT_MSG(routerLink3.GetN() == 2, "Router link 3 should have exactly 2 devices");
}

void TestIpAddressAssignment()
{
    NodeContainer routers;
    routers.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer routerLink1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer routerLink2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer routerLink3 = p2p.Install(routers.Get(0), routers.Get(2));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iRouter1 = ipv4.Assign(routerLink1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iRouter2 = ipv4.Assign(routerLink2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iRouter3 = ipv4.Assign(routerLink3);

    NS_ASSERT_MSG(iRouter1.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 1 should have a valid IP");
    NS_ASSERT_MSG(iRouter2.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 2 should have a valid IP");
    NS_ASSERT_MSG(iRouter3.GetAddress(0) != Ipv4Address("0.0.0.0"), "Router 3 should have a valid IP");
}

void TestRipRouting()
{
    NodeContainer routers;
    routers.Create(3);

    RipHelper ripRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ripRouting);
    internet.Install(routers);

    Ptr<Ipv4RoutingProtocol> ripProtocol = routers.Get(0)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ripProtocol != nullptr, "RIP routing should be enabled on router 0");

    ripProtocol = routers.Get(1)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ripProtocol != nullptr, "RIP routing should be enabled on router 1");

    ripProtocol = routers.Get(2)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT_MSG(ripProtocol != nullptr, "RIP routing should be enabled on router 2");
}

void TestUdpApplications()
{
    NodeContainer routers;
    routers.Create(3);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on Router 2");

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.2.2"), 9);
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
    routers.Create(3);

    PointToPointHelper p2p;
    NetDeviceContainer routerLink1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer routerLink2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer routerLink3 = p2p.Install(routers.Get(0), routers.Get(2));

    p2p.EnablePcapAll("dvr-routing");

    // Check if tracing is enabled
    NS_ASSERT_MSG(system("ls dvr-routing-0-0.pcap") == 0, "PCAP trace file should be generated");
}

void TestSimulationExecution()
{
    NodeContainer routers;
    routers.Create(3);

    Simulator::Run();
    NS_ASSERT_MSG(Simulator::Now().GetSeconds() >= 10.0, "Simulation should run for at least 10 seconds");

    Simulator::Destroy();
}

void TestSimulationCleanUp()
{
    NodeContainer routers;
    routers.Create(3);

    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(Simulator::GetRemainingTime() == Time(0), "Simulation should be properly cleaned up");
}

