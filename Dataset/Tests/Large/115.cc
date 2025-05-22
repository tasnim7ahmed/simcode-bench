void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Testing Node Creation...");
    NS_ASSERT(nodes.GetN() == 2);  // Ensure two nodes are created
}

void TestDeviceInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    NS_LOG_INFO("Testing Device Installation...");
    NS_ASSERT(devices.GetN() == 2);  // Ensure two devices are installed
}

void TestMobilityInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-25, 25, -25, 25)));
    mobility.Install(nodes);

    NS_LOG_INFO("Testing Mobility Installation...");

    Ptr<MobilityModel> mobilityModelNode0 = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mobilityModelNode1 = nodes.Get(1)->GetObject<MobilityModel>();

    NS_ASSERT(mobilityModelNode0 != nullptr);  // Ensure mobility model is installed for node 0
    NS_ASSERT(mobilityModelNode1 != nullptr);  // Ensure mobility model is installed for node 1
}

void TestIpv4AddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Testing IPv4 Address Assignment...");

    Ipv4Address node0Address = interfaces.GetAddress(0);
    Ipv4Address node1Address = interfaces.GetAddress(1);

    NS_ASSERT(node0Address.IsValid());  // Ensure node 0 has a valid IPv4 address
    NS_ASSERT(node1Address.IsValid());  // Ensure node 1 has a valid IPv4 address
}

void TestUdpApplicationSetup()
{
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9; // well-known echo port number
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(100.0));

    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(100.0));

    NS_LOG_INFO("Testing UDP Application Setup...");

    NS_ASSERT(serverApps.GetN() == 1);  // Ensure the server application is installed
    NS_ASSERT(clientApps.GetN() == 1);  // Ensure the client application is installed
}

void TestFlowMonitor()
{
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(100.0));

    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(100.0));

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    NS_LOG_INFO("Testing Flow Monitoring...");

    NS_ASSERT(flowMonitor != nullptr);  // Ensure FlowMonitor is successfully installed
}

void TestPerformanceMetrics()
{
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(100.0));

    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(100.0));

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Run();

    uint32_t packetsDelivered = 0;
    uint32_t totalPackets = 0;
    double totalDelay = 0;

    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto &it : stats)
    {
        if (it.second.rxPackets > 0)
        {
            packetsDelivered += it.second.rxPackets;
            totalPackets += it.second.txPackets;
            totalDelay += (it.second.delaySum.ToDouble(ns3::Time::S) / it.second.rxPackets);
        }
    }

    double packetDeliveryRatio = (double)packetsDelivered / totalPackets;
    double averageEndToEndDelay = totalDelay / packetsDelivered;
    double throughput = (double)packetsDelivered * 1024 * 8 / 100.0 / 1000000;

    NS_LOG_INFO("Testing Performance Metrics...");
    NS_LOG_INFO("Packet Delivery Ratio: " << packetDeliveryRatio);
    NS_LOG_INFO("Average End-to-End Delay: " << averageEndToEndDelay);
    NS_LOG_INFO("Throughput: " << throughput);

    NS_ASSERT(packetsDelivered > 0);  // Ensure that some packets were delivered
    NS_ASSERT(totalPackets > 0);      // Ensure that some packets were transmitted
}

int main(int argc, char *argv[])
{
    // Enable verbose logging if required
    bool verbose = true;
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("AdhocNetworkExample", LOG_LEVEL_INFO);
    }

    // Run the individual tests
    TestNodeCreation();
    TestDeviceInstallation();
    TestMobilityInstallation();
    TestIpv4AddressAssignment();
    TestUdpApplicationSetup();
    TestFlowMonitor();
    TestPerformanceMetrics();

    return 0;
}

