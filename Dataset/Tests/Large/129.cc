void TestNodeCreation()
{
    NodeContainer router;
    router.Create(1);

    NodeContainer vlan1Hosts, vlan2Hosts, vlan3Hosts;
    vlan1Hosts.Create(2);
    vlan2Hosts.Create(2);
    vlan3Hosts.Create(2);

    NodeContainer vlan1Switch, vlan2Switch, vlan3Switch;
    vlan1Switch.Create(1);
    vlan2Switch.Create(1);
    vlan3Switch.Create(1);

    NS_ASSERT_MSG(router.GetN() == 1, "Router node should be created.");
    NS_ASSERT_MSG(vlan1Hosts.GetN() == 2, "VLAN1 should have exactly 2 hosts.");
    NS_ASSERT_MSG(vlan2Hosts.GetN() == 2, "VLAN2 should have exactly 2 hosts.");
    NS_ASSERT_MSG(vlan3Hosts.GetN() == 2, "VLAN3 should have exactly 2 hosts.");
    NS_ASSERT_MSG(vlan1Switch.GetN() == 1, "VLAN1 switch should be created.");
    NS_ASSERT_MSG(vlan2Switch.GetN() == 1, "VLAN2 switch should be created.");
    NS_ASSERT_MSG(vlan3Switch.GetN() == 1, "VLAN3 switch should be created.");
}

void TestPointToPointLinks()
{
    NodeContainer router, vlan1Switch, vlan2Switch, vlan3Switch;
    router.Create(1);
    vlan1Switch.Create(1);
    vlan2Switch.Create(1);
    vlan3Switch.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer vlan1RouterDevices = p2p.Install(router.Get(0), vlan1Switch.Get(0));
    NetDeviceContainer vlan2RouterDevices = p2p.Install(router.Get(0), vlan2Switch.Get(0));
    NetDeviceContainer vlan3RouterDevices = p2p.Install(router.Get(0), vlan3Switch.Get(0));

    NS_ASSERT_MSG(vlan1RouterDevices.GetN() == 2, "Router should have a link to VLAN1 switch.");
    NS_ASSERT_MSG(vlan2RouterDevices.GetN() == 2, "Router should have a link to VLAN2 switch.");
    NS_ASSERT_MSG(vlan3RouterDevices.GetN() == 2, "Router should have a link to VLAN3 switch.");
}

void TestIpAddressAssignment()
{
    NodeContainer router;
    router.Create(1);

    PointToPointHelper p2p;
    NodeContainer vlan1Switch, vlan2Switch, vlan3Switch;
    vlan1Switch.Create(1);
    vlan2Switch.Create(1);
    vlan3Switch.Create(1);

    NetDeviceContainer vlan1RouterDevices = p2p.Install(router.Get(0), vlan1Switch.Get(0));
    NetDeviceContainer vlan2RouterDevices = p2p.Install(router.Get(0), vlan2Switch.Get(0));
    NetDeviceContainer vlan3RouterDevices = p2p.Install(router.Get(0), vlan3Switch.Get(0));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = ipv4.Assign(vlan1RouterDevices.Get(0));

    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = ipv4.Assign(vlan2RouterDevices.Get(0));

    ipv4.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = ipv4.Assign(vlan3RouterDevices.Get(0));

    NS_ASSERT_MSG(vlan1Interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "VLAN1 should have a valid IP.");
    NS_ASSERT_MSG(vlan2Interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "VLAN2 should have a valid IP.");
    NS_ASSERT_MSG(vlan3Interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "VLAN3 should have a valid IP.");
}

void TestDhcpConfiguration()
{
    NodeContainer router;
    router.Create(1);

    PointToPointHelper p2p;
    NodeContainer vlan1Switch, vlan1Hosts;
    vlan1Switch.Create(1);
    vlan1Hosts.Create(2);

    NetDeviceContainer vlan1RouterDevices = p2p.Install(router.Get(0), vlan1Switch.Get(0));

    DhcpHelper dhcp;
    dhcp.SetRouter(router.Get(0));
    dhcp.InstallDhcpServer(vlan1RouterDevices.Get(1), Ipv4Address("192.168.1.1"));
    dhcp.InstallDhcpClient(vlan1Hosts);

    NS_ASSERT_MSG(router.Get(0)->GetObject<Ipv4>() != nullptr, "Router should support IPv4 for DHCP.");
}

void TestStaticRouting()
{
    NodeContainer router;
    router.Create(1);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> routerStaticRouting = ipv4RoutingHelper.GetStaticRouting(router.Get(0)->GetObject<Ipv4>());

    routerStaticRouting->SetDefaultRoute("192.168.1.1", 1);
    routerStaticRouting->SetDefaultRoute("192.168.2.1", 2);
    routerStaticRouting->SetDefaultRoute("192.168.3.1", 3);

    NS_ASSERT_MSG(routerStaticRouting != nullptr, "Router should have static routing enabled.");
}

void TestUdpCommunication()
{
    NodeContainer vlan1Hosts, vlan3Hosts;
    vlan1Hosts.Create(1);
    vlan3Hosts.Create(1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(vlan3Hosts.Get(0));
    serverApp.Start(Seconds(2.0));
    serverApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on VLAN3 host.");

    UdpEchoClientHelper echoClient(Ipv4Address("192.168.3.2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(vlan1Hosts.Get(0));
    clientApp.Start(Seconds(3.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed on VLAN1 host.");
}

