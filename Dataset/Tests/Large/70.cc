void TestCommandLineParsing() {
    CommandLine cmd;
    uint32_t nLeaf = 10;
    uint32_t maxPackets = 100;
    bool modeBytes = false;
    uint32_t queueDiscLimitPackets = 1000;
    double minTh = 5;
    double maxTh = 15;
    uint32_t pktSize = 512;
    std::string appDataRate = "10Mbps";
    std::string queueDiscType = "RED";
    
    cmd.AddValue("nLeaf", "Number of left and right side leaf nodes", nLeaf);
    cmd.AddValue("maxPackets", "Max Packets allowed in the device queue", maxPackets);
    cmd.AddValue("queueDiscLimitPackets", "Max Packets allowed in the queue disc", queueDiscLimitPackets);
    cmd.AddValue("queueDiscType", "Set Queue disc type to RED or NLRED", queueDiscType);
    cmd.Parse(0, nullptr);
    
    NS_TEST_ASSERT_MSG_EQ(nLeaf, 10, "Incorrect nLeaf value");
    NS_TEST_ASSERT_MSG_EQ(queueDiscType, "RED", "Queue Disc Type is not RED");
}

void TestQueueDiscConfiguration() {
    uint32_t pktSize = 512;
    uint32_t maxPackets = 100;
    bool modeBytes = false;
    double minTh = 5;
    double maxTh = 15;

    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(pktSize));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxPackets)));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));

    QueueSize queueSize = Config::GetDefaultValue("ns3::RedQueueDisc::MaxSize").Get<QueueSize>();
    NS_TEST_ASSERT_MSG_EQ(queueSize.GetUnit(), QueueSizeUnit::PACKETS, "Queue Size unit mismatch");

    double configuredMinTh = Config::GetDefaultValue("ns3::RedQueueDisc::MinTh").Get<DoubleValue>().Get();
    NS_TEST_ASSERT_MSG_EQ(configuredMinTh, minTh, "Incorrect minimum threshold");
}

void TestNetworkSetup() {
    uint32_t nLeaf = 10;
    PointToPointDumbbellHelper d(nLeaf, PointToPointHelper(), nLeaf, PointToPointHelper(), PointToPointHelper());

    InternetStackHelper stack;
    stack.Install(d.GetLeft());
    stack.Install(d.GetRight());

    TrafficControlHelper tchBottleneck;
    QueueDiscContainer queueDiscs = tchBottleneck.Install(d.GetLeft()->GetDevice(0));
    NS_TEST_ASSERT_MSG_EQ(queueDiscs.Get(0)->GetTypeId().GetName(), "ns3::RedQueueDisc", "Incorrect queue disc type");

    // Check if the link bandwidth and delay are configured
    std::string bandwidth = Config::GetDefaultValue("ns3::RedQueueDisc::LinkBandwidth").Get<StringValue>().Get();
    std::string delay = Config::GetDefaultValue("ns3::RedQueueDisc::LinkDelay").Get<StringValue>().Get();
    
    NS_TEST_ASSERT_MSG_EQ(bandwidth, "1Mbps", "Incorrect link bandwidth");
    NS_TEST_ASSERT_MSG_EQ(delay, "50ms", "Incorrect link delay");
}

void TestPacketDropBehavior() {
    uint32_t nLeaf = 10;
    PointToPointDumbbellHelper d(nLeaf, PointToPointHelper(), nLeaf, PointToPointHelper(), PointToPointHelper());
    
    TrafficControlHelper tchBottleneck;
    QueueDiscContainer queueDiscs = tchBottleneck.Install(d.GetLeft()->GetDevice(0));
    
    Simulator::Run();

    QueueDisc::Stats stats = queueDiscs.Get(0)->GetStats();
    
    uint32_t unforcedDrops = stats.GetNDroppedPackets(RedQueueDisc::UNFORCED_DROP);
    uint32_t internalQueueDrops = stats.GetNDroppedPackets(QueueDisc::INTERNAL_QUEUE_DROP);
    
    NS_TEST_ASSERT_MSG_EQ(unforcedDrops > 0, true, "Expected some unforced drops");
    NS_TEST_ASSERT_MSG_EQ(internalQueueDrops, 0, "Expected zero drops due to queue full");
}

void TestIpAssignmentAndApplicationFlow() {
    uint32_t nLeaf = 10;
    PointToPointDumbbellHelper d(nLeaf, PointToPointHelper(), nLeaf, PointToPointHelper(), PointToPointHelper());

    d.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
                          Ipv4AddressHelper("10.2.1.0", "255.255.255.0"),
                          Ipv4AddressHelper("10.3.1.0", "255.255.255.0"));

    Ipv4InterfaceContainer interfaces = d.GetLeftIpv4Address();
    for (uint32_t i = 0; i < nLeaf; ++i) {
        Ipv4Address addr = interfaces.GetAddress(i);
        NS_TEST_ASSERT_MSG_EQ(addr.GetType(), Ipv4Address::GetType(), "IP address type mismatch");
    }

    // Check that applications are installed and running
    ApplicationContainer sinkApps = d.GetLeft()->GetApplicationContainer();
    NS_TEST_ASSERT_MSG_EQ(sinkApps.GetN(), nLeaf, "Number of applications on the left side mismatch");
}

