void TestNodeCreation() {
  NodeContainer nodes;
  nodes.Create(2);

  NS_ASSERT_EQUAL(nodes.GetN(), 2); // Assert that two nodes are created
}

void TestWifiChannel() {
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NS_ASSERT_NOT_NULL(phy.GetChannel()); // Assert that the channel is created
}

void TestMobilityModel() {
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

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
                            "Bounds", RectangleValue(Rectangle(-25, 25, -25, 25)));
  mobility.Install(nodes);

  // Check if mobility is correctly installed
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
    NS_ASSERT_NOT_NULL(mobilityModel); // Assert that mobility model is assigned to node
  }
}

void TestApplicationSetup() {
  NodeContainer nodes;
  nodes.Create(2);

  UdpEchoServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));

  UdpEchoClientHelper client(Ipv4Address("192.9.39.1"), 9);
  client.SetAttribute("MaxPackets", UintegerValue(50));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));

  // Check that applications were installed
  NS_ASSERT_TRUE(serverApps.GetN() > 0);
  NS_ASSERT_TRUE(clientApps.GetN() > 0);
}

void TestFlowMonitor() {
  NodeContainer nodes;
  nodes.Create(2);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(100.0));
  
  Simulator::Run();

  // Ensure that the flow monitor has recorded stats
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  NS_ASSERT_FALSE(stats.empty()); // Assert that stats are collected

  Simulator::Destroy();
}

void TestPacketDeliveryRatio() {
  NodeContainer nodes;
  nodes.Create(2);

  // Setup applications
  UdpEchoServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));

  UdpEchoClientHelper client(Ipv4Address("192.9.39.1"), 9);
  client.SetAttribute("MaxPackets", UintegerValue(50));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));

  // Flow monitoring
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(100.0));

  Simulator::Run();
  
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (const auto& flowStat : stats) {
    uint32_t packetsDelivered = flowStat.second.rxPackets;
    uint32_t totalPackets = flowStat.second.txPackets;

    double packetDeliveryRatio = (double)packetsDelivered / totalPackets;
    NS_ASSERT_TRUE(packetDeliveryRatio >= 0.0 && packetDeliveryRatio <= 1.0); // Assert that delivery ratio is within valid range
  }

  Simulator::Destroy();
}

void TestThroughputCalculation() {
  NodeContainer nodes;
  nodes.Create(2);

  UdpEchoServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));

  UdpEchoClientHelper client(Ipv4Address("192.9.39.1"), 9);
  client.SetAttribute("MaxPackets", UintegerValue(50));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));

  // Flow monitoring
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(100.0));

  Simulator::Run();
  
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (const auto& flowStat : stats) {
    uint32_t packetsDelivered = flowStat.second.rxPackets;
    double throughput = (double)packetsDelivered * 1024 * 8 / (100.0 - 0.0) / 1000000;
    NS_ASSERT_TRUE(throughput >= 0.0); // Assert that throughput is non-negative
  }

  Simulator::Destroy();
}

void TestEndToEndDelay() {
  NodeContainer nodes;
  nodes.Create(2);

  UdpEchoServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));

  UdpEchoClientHelper client(Ipv4Address("192.9.39.1"), 9);
  client.SetAttribute("MaxPackets", UintegerValue(50));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));

  // Flow monitoring
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(100.0));

  Simulator::Run();
  
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (const auto& flowStat : stats) {
    double totalDelay = flowStat.second.delaySum.ToDouble(Time::S) / flowStat.second.rxPackets;
    NS_ASSERT_TRUE(totalDelay >= 0.0); // Assert that delay is non-negative
  }

  Simulator::Destroy();
}

