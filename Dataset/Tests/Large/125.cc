void TestNodeCreation()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    // Verify the number of nodes created
    NS_ASSERT(centralRouter.GetN() == 1);  // One central router
    NS_ASSERT(edgeRouters.GetN() == 4);    // Four edge routers
}

void TestPointToPointLinks()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(centralRouter.Get(0), edgeRouters.Get(i));
    }

    // Verify the number of devices created (should be 4 links)
    NS_ASSERT(devices[0].GetN() == 2);  // Two devices for each link (central router and edge router)
    NS_ASSERT(devices[1].GetN() == 2);
    NS_ASSERT(devices[2].GetN() == 2);
    NS_ASSERT(devices[3].GetN() == 2);
}

void TestIpAddressAssignment()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(centralRouter.Get(0), edgeRouters.Get(i));
    }

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = ipv4.Assign(devices[i]);
    }

    // Verify that the IP addresses are assigned correctly
    for (int i = 0; i < 4; ++i)
    {
        NS_ASSERT(interfaces[i].GetAddress(0) != Ipv4Address("0.0.0.0"));
        NS_ASSERT(interfaces[i].GetAddress(1) != Ipv4Address("0.0.0.0"));
    }
}

void TestRipRouting()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    RipHelper ripRouting;

    InternetStackHelper internet;
    internet.SetRoutingHelper(ripRouting); // Set RIP as the routing protocol
    internet.Install(centralRouter);
    internet.Install(edgeRouters);

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = ipv4.Assign(devices[i]);
    }

    // Add RIP network routes
    for (int i = 0; i < 4; ++i)
    {
        ripRouting.AddNetworkRouteTo(centralRouter.Get(0), interfaces[i].GetAddress(0), interfaces[i].GetAddress(1), 1);
    }

    // Verify that RIP routing is enabled
    Ptr<Ipv4RoutingProtocol> ripProtocol = centralRouter.Get(0)->GetObject<Ipv4RoutingProtocol>();
    NS_ASSERT(ripProtocol != nullptr);
}

void TestUdpApplications()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(edgeRouters.Get(0)); // Server on the first edge router
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9); // Client on the second edge router
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(edgeRouters.Get(1)); 
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Verify that server and client applications are installed
    NS_ASSERT(serverApps.GetN() == 1);
    NS_ASSERT(clientApps.GetN() == 1);
}
void TestPacketTracing()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(centralRouter.Get(0), edgeRouters.Get(i));
    }

    p2p.EnablePcapAll("rip-routing");

    // Verify that the pcap file is generated
    NS_ASSERT(system("ls rip-routing-0-0.pcap") == 0);  // Ensure the capture file exists
}

void TestSimulationExecution()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    // Run the simulation
    Simulator::Run();

    // Ensure the simulation runs for the expected duration (from 1.0 to 10.0 seconds)
    NS_ASSERT(Simulator::Now().GetSeconds() >= 10.0);

    Simulator::Destroy();
}
void TestSimulationCleanUp()
{
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    // Run the simulation
    Simulator::Run();

    // Ensure the simulation is properly cleaned up
    NS_ASSERT(Simulator::GetRemainingTime() == Time(0));  // No time left after running
    Simulator::Destroy();
}



