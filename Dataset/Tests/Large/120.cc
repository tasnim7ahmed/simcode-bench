void TestNodeCreation()
{
    NodeContainer nodes;
    int size = 10; // Example value

    nodes.Create(size);
    NS_ASSERT(nodes.GetN() == size);
}

void TestWifiConfiguration()
{
    NodeContainer nodes;
    nodes.Create(10);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    NS_ASSERT(devices.GetN() == nodes.GetN()); // Check if devices were created for each node
}

void TestMobilityModel()
{
    NodeContainer nodes;
    nodes.Create(10);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(nodes);

    Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT(mobilityModel != nullptr); // Ensure mobility model is installed
}

void TestAodvRouting()
{
    NodeContainer nodes;
    nodes.Create(10);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    stack.Install(nodes);

    Ipv4RoutingTableHelper routingHelper;
    routingHelper.PrintRoutingTableAllAt(Seconds(2.0)); // Print routing table
}

void TestUdpApplications()
{
    NodeContainer nodes;
    nodes.Create(10);

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(100.0));

    UdpEchoClientHelper client(Ipv4Address("192.2.22.1"), port);
    client.SetAttribute("MaxPackets", UintegerValue(50));
    client.SetAttribute("Interval", TimeValue(Seconds(4.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(100.0));
    
    // Check that server and client apps are installed
    NS_ASSERT(serverApps.GetN() > 0);
    NS_ASSERT(clientApps.GetN() > 0);
}

void TestFlowMonitor()
{
    NodeContainer nodes;
    nodes.Create(10);

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
    nodes.Create(10);

    int totalTime = 100;
    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    NS_ASSERT(Simulator::Now().GetSeconds() == totalTime);
    Simulator::Destroy();
}

void TestPcapOutput()
{
    NodeContainer nodes;
    nodes.Create(10);

    bool pcap = true; // Enable PCAP
    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    if (pcap)
    {
        phy.EnablePcapAll(std::string("aodv"));
    }

    // Verify that PCAP files are generated
    NS_ASSERT(system("ls aodv-*.pcap") == 0); // Assuming PCAP files are generated with "aodv" in their names
}

void TestUpdateNodePositions()
{
    NodeContainer nodes;
    nodes.Create(10);

    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    UpdateNodePositions(nodes);

    // Ensure that the positions are updated
    Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT(mobilityModel != nullptr);
    Vector position = mobilityModel->GetPosition();
    NS_ASSERT(position.x != 0.0 || position.y != 0.0); // Assuming the position has changed
}

