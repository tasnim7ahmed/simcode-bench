void TestNodeCreation()
{
    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer teamA, teamB, teamC;
    teamA.Create(5);
    teamB.Create(5);
    teamC.Create(5);

    // Verify number of nodes in each container
    NS_ASSERT(centralCommand.GetN() == 1);
    NS_ASSERT(medicalUnits.GetN() == 3);
    NS_ASSERT(teamA.GetN() == 5);
    NS_ASSERT(teamB.GetN() == 5);
    NS_ASSERT(teamC.GetN() == 5);
}

void TestWifiConfiguration()
{
    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));

    Ssid ssid = Ssid("DisasterRecoveryNet");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer medicalDevices = wifi.Install(phy, mac, medicalUnits);

    NS_ASSERT(medicalDevices.GetN() == medicalUnits.GetN());
}

void TestMobilityModel()
{
    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(centralCommand);
    mobility.Install(medicalUnits);

    Ptr<MobilityModel> mobilityModel = medicalUnits.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT(mobilityModel != nullptr);  // Ensure that mobility model is installed correctly
}

void TestAodvRouting()
{
    NodeContainer teamA, teamB, teamC;
    teamA.Create(5);
    teamB.Create(5);
    teamC.Create(5);

    AodvHelper aodv;
    InternetStackHelper stackA, stackB, stackC;
    stackA.SetRoutingHelper(aodv);
    stackB.SetRoutingHelper(aodv);
    stackC.SetRoutingHelper(aodv);
    stackA.Install(teamA);
    stackB.Install(teamB);
    stackC.Install(teamC);

    // Verify that the AODV routing protocol has been installed for each team
    NS_ASSERT(teamA.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol() != nullptr);
    NS_ASSERT(teamB.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol() != nullptr);
    NS_ASSERT(teamC.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol() != nullptr);
}

void TestUdpApplications()
{
    NodeContainer teamA;
    teamA.Create(5);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(teamA.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(100.0));

    UdpEchoClientHelper echoClient(teamA.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = echoClient.Install(teamA.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(100.0));

    // Ensure server and client apps are installed
    NS_ASSERT(serverApps.GetN() > 0);
    NS_ASSERT(clientApps.GetN() > 0);
}

void TestIpAssignment()
{
    NodeContainer teamA;
    teamA.Create(5);

    WifiHelper wifi;
    WifiMacHelper mac;
    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, teamA);

    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify the first node in teamA has an IP address
    NS_ASSERT(interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"));
}

void TestFlowMonitor()
{
    NodeContainer nodes;
    nodes.Create(5);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    Simulator::Run();

    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    NS_ASSERT(stats.size() > 0); // Ensure we have some flow statistics

    // Print the stats for verification
    for (auto &it : stats)
    {
        std::cout << "Flow " << it.first << " Rx Packets: " << it.second.rxPackets << std::endl;
    }

    Simulator::Destroy();
}

void TestSimulationTime()
{
    NodeContainer nodes;
    nodes.Create(5);

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    NS_ASSERT(Simulator::Now().GetSeconds() == 100.0);  // Ensure the simulation ends at 100 seconds
    Simulator::Destroy();
}

void TestAnimationOutput()
{
    AnimationInterface anim("final.xml");
    
    // Verify that the animation file has been created
    NS_ASSERT(system("ls final.xml") == 0); // Ensure that the animation output file exists
}

void TestRtsCtsThreshold()
{
    bool enableCtsRts = true;
    UintegerValue ctsThr = (enableCtsRts ? UintegerValue(100) : UintegerValue(2200));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);
    
    // Verify RTS/CTS threshold configuration
    uint32_t rtsThreshold = Config::GetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold").GetValue();
    NS_ASSERT(rtsThreshold == 100);  // Verify that RTS/CTS is enabled with threshold 100
}

