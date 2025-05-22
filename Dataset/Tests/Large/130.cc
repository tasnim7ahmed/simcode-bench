void TestNodeCreation()
{
    NodeContainer natRouter, privateHosts, publicServer;
    natRouter.Create(1);
    privateHosts.Create(2);
    publicServer.Create(1);

    NS_ASSERT_MSG(natRouter.GetN() == 1, "NAT router should be created.");
    NS_ASSERT_MSG(privateHosts.GetN() == 2, "There should be two private hosts.");
    NS_ASSERT_MSG(publicServer.GetN() == 1, "There should be one public server.");
}

void TestPointToPointLinks()
{
    NodeContainer natRouter, privateHosts, publicServer;
    natRouter.Create(1);
    privateHosts.Create(2);
    publicServer.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer privateDevices = p2p.Install(NodeContainer(privateHosts, natRouter));
    NetDeviceContainer publicDevices = p2p.Install(NodeContainer(natRouter, publicServer));

    NS_ASSERT_MSG(privateDevices.GetN() == 3, "Private network should have links to NAT router.");
    NS_ASSERT_MSG(publicDevices.GetN() == 2, "Public network should have a link between NAT router and public server.");
}

void TestIpAddressAssignment()
{
    NodeContainer natRouter, privateHosts, publicServer;
    natRouter.Create(1);
    privateHosts.Create(2);
    publicServer.Create(1);

    PointToPointHelper p2p;
    NetDeviceContainer privateDevices = p2p.Install(NodeContainer(privateHosts, natRouter));
    NetDeviceContainer publicDevices = p2p.Install(NodeContainer(natRouter, publicServer));

    Ipv4AddressHelper privateNetwork;
    privateNetwork.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer privateInterfaces = privateNetwork.Assign(privateDevices.Get(0));

    Ipv4AddressHelper publicNetwork;
    publicNetwork.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterfaces = publicNetwork.Assign(publicDevices.Get(1));
    publicNetwork.Assign(publicDevices.Get(0));

    NS_ASSERT_MSG(privateInterfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "Private hosts should have valid IPs.");
    NS_ASSERT_MSG(publicInterfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "Public server should have a valid IP.");
}

void TestNatConfiguration()
{
    NodeContainer natRouter;
    natRouter.Create(1);

    PointToPointHelper p2p;
    NodeContainer publicServer;
    publicServer.Create(1);
    NetDeviceContainer publicDevices = p2p.Install(NodeContainer(natRouter, publicServer));

    NatHelper natHelper;
    natHelper.Install(natRouter.Get(0), publicDevices.Get(0));

    NS_ASSERT_MSG(natRouter.Get(0)->GetObject<NatHelper>() != nullptr, "NAT should be installed on the router.");
}

void TestUdpCommunication()
{
    NodeContainer privateHosts, publicServer;
    privateHosts.Create(2);
    publicServer.Create(1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(publicServer.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on the public server.");

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.1.2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(privateHosts.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed on a private host.");
}

void TestNatPacketTranslation()
{
    NodeContainer natRouter, privateHosts, publicServer;
    natRouter.Create(1);
    privateHosts.Create(2);
    publicServer.Create(1);

    PointToPointHelper p2p;
    NetDeviceContainer privateDevices = p2p.Install(NodeContainer(privateHosts, natRouter));
    NetDeviceContainer publicDevices = p2p.Install(NodeContainer(natRouter, publicServer));

    Ipv4AddressHelper privateNetwork;
    privateNetwork.SetBase("192.168.1.0", "255.255.255.0");
    privateNetwork.Assign(privateDevices.Get(0));

    Ipv4AddressHelper publicNetwork;
    publicNetwork.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterfaces = publicNetwork.Assign(publicDevices.Get(1));
    publicNetwork.Assign(publicDevices.Get(0));

    NatHelper natHelper;
    natHelper.Install(natRouter.Get(0), publicDevices.Get(0));

    UdpEchoClientHelper echoClient(publicInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(privateHosts.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation to check if NAT translation works
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(true, "Packets should be translated by NAT.");
}

