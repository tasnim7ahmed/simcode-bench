void TestNodeCreation()
{
    // Create 3 mobile nodes
    NodeContainer manetNodes;
    manetNodes.Create(3);

    // Test if 3 nodes are created
    NS_TEST_ASSERT_MSG_EQ(manetNodes.GetN(), 3, "Node creation failed");
}

void TestWiFiConfiguration()
{
    NodeContainer manetNodes;
    manetNodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("manet-ssid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, manetNodes);

    // Test if WiFi devices are installed correctly
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "WiFi installation failed");
}

void TestIPAssignment()
{
    NodeContainer manetNodes;
    manetNodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("manet-ssid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, manetNodes);

    InternetStackHelper stack;
    stack.Install(manetNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Test if IP addresses are assigned
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 3, "IP address assignment failed");
}

void TestApplicationInstallation()
{
    NodeContainer manetNodes;
    manetNodes.Create(3);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(manetNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.1.1"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(manetNodes.Get(2));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Test if server and client applications are installed
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP Echo Server installation failed");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP Echo Client installation failed");
}

void TestMobilityModel()
{
    NodeContainer manetNodes;
    manetNodes.Create(3);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(manetNodes);

    // Test if mobility model is installed
    Ptr<MobilityModel> mobilityModel = manetNodes.Get(0)->GetObject<MobilityModel>();
    NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model installation failed");
}

void TestFlowMonitor()
{
    NodeContainer manetNodes;
    manetNodes.Create(3);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Start the simulation
    Simulator::Run();

    // Capture flow monitor data
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("manet-flowmon.xml", true, true);

    // Test if flow monitor is capturing data
    NS_TEST_ASSERT_MSG_EQ(monitor->GetFlowCount(), 1, "Flow monitor did not capture the expected number of flows");
}

void TestSimulationRun()
{
    NodeContainer manetNodes;
    manetNodes.Create(3);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Test if simulation runs successfully
    NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation run failed");
}

void TestNodeCreation();
void TestWiFiConfiguration();
void TestIPAssignment();
void TestApplicationInstallation();
void TestMobilityModel();
void TestFlowMonitor();
void TestSimulationRun();

int main()
{
    TestNodeCreation();
    TestWiFiConfiguration();
    TestIPAssignment();
    TestApplicationInstallation();
    TestMobilityModel();
    TestFlowMonitor();
    TestSimulationRun();

    return 0;
}
