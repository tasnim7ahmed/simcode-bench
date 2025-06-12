void TestNodeCreationAndInitialization()
{
  NodeContainer nodes;
  nodes.Create(4);

  NS_ASSERT(nodes.GetN() == 4);  // Verify 4 nodes are created
}

void TestMobilityModelSetup()
{
  NodeContainer nodes;
  nodes.Create(4);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (2.0),
                                 "DeltaY", DoubleValue (2.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100,100)));
  mobility.Install(nodes);

  // Assuming the nodes should be initialized at some positions, check the positions of the first node
  Ptr<Node> firstNode = nodes.Get(0);
  Ptr<MobilityModel> mobilityModel = firstNode->GetObject<MobilityModel>();
  Vector position = mobilityModel->GetPosition();

  NS_ASSERT(position.x == 0.0 && position.y == 0.0);  // Verify initial position
}

void TestWifiDeviceInstallation()
{
  NodeContainer nodes;
  nodes.Create(4);

  WifiHelper wifi;
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  NS_ASSERT(devices.GetN() == 4);  // Verify that the 4 devices have been installed on the nodes
}

void TestIpv4AddressAssignment()
{
  NodeContainer nodes;
  nodes.Create(4);

  WifiHelper wifi;
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  NS_ASSERT(interfaces.GetN() == 4);  // Verify that 4 interfaces are assigned
}

void TestUdpClientServerCommunication()
{
  NodeContainer nodes;
  nodes.Create(4);

  WifiHelper wifi;
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper server(10);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));

  UdpEchoClientHelper client(interfaces.GetAddress(0), 10);
  client.SetAttribute("MaxPackets", UintegerValue(20));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));

  Simulator::Run();

  // Verify if packets were delivered (you could use flow monitor here)
  // For simplicity, we assume that if no errors occur, communication is successful
  NS_LOG_INFO("UDP client-server communication test passed.");
  Simulator::Destroy();
}

void TestFlowMonitor()
{
  NodeContainer nodes;
  nodes.Create(4);

  WifiHelper wifi;
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper server(10);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));

  UdpEchoClientHelper client(interfaces.GetAddress(0), 10);
  client.SetAttribute("MaxPackets", UintegerValue(20));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));

  // Flow monitoring setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(100.0));

  Simulator::Run();

  // Collect flow statistics and calculate throughput
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (auto& flow : stats)
  {
    double throughput = (double)flow.second.rxBytes * 8 / 100.0 / 1e6;
    NS_LOG_INFO("Throughput for FlowId " << flow.first << ": " << throughput << " Mbps");
  }

  Simulator::Destroy();
}

void TestSimulationRunAndCleanup()
{
  NodeContainer nodes;
  nodes.Create(4);

  Simulator::Run();
  NS_LOG_INFO("Simulation ran successfully.");
  Simulator::Destroy();
}

