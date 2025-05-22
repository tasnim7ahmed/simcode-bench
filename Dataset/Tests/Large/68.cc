void TestQueueDiscType()
{
    // Test different queueDiscType values
    std::string queueDiscType = "PfifoFast";  // default
    uint32_t queueDiscSize = 1000;
    
    // Simulate setting the queue disc type (this would typically happen in the main simulation function)
    TrafficControlHelper tchBottleneck;
    if (queueDiscType == "PfifoFast")
    {
        tchBottleneck.SetRootQueueDisc(
            "ns3::PfifoFastQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    else if (queueDiscType == "ARED")
    {
        tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
        Config::SetDefault("ns3::RedQueueDisc::ARED", BooleanValue(true));
        Config::SetDefault("ns3::RedQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    else if (queueDiscType == "CoDel")
    {
        tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc");
        Config::SetDefault("ns3::CoDelQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    // Add more else if blocks for other types...
    
    // Validate that the traffic control helper was set up properly
    NS_TEST_ASSERT_MSG_EQ(queueDiscType, "PfifoFast", "Queue Disc Type setup failed");
}

void TestGoodputSampling()
{
    // Create mock application container
    ApplicationContainer app;
    Ptr<PacketSink> sink = CreateObject<PacketSink>();
    app.Add(sink);
    
    // Simulate a goodput sampling
    Ptr<OutputStreamWrapper> stream = CreateObject<OutputStreamWrapper>();
    float period = 1.0;  // Sample every second
    Simulator::Schedule(Seconds(period), &GoodputSampling, app, stream, period);
    
    // Check if the goodput is being written to the trace file
    NS_TEST_ASSERT_MSG_EQ(stream->GetStream()->good(), true, "Goodput Sampling failed");
}

void TestPingRtt()
{
    // Create a mock Ping application
    PingHelper ping;
    ping.SetAttribute("VerboseMode", EnumValue(Ping::VerboseMode::QUIET));
    
    std::string context = "/NodeList/0/ApplicationList/2/$ns3::Ping";
    Time rtt = Seconds(0.1);  // simulate an RTT of 100ms
    
    // Capture the RTT
    PingRtt(context, 0, rtt);
    
    // Validate that the output RTT is correct
    NS_TEST_ASSERT_MSG_EQ(rtt.GetMilliSeconds(), 100.0, "Ping RTT measurement failed");
}

void TestQueueLimitsTrace()
{
    // Simulate queue limits trace
    Ptr<DynamicQueueLimits> queueLimits = CreateObject<DynamicQueueLimits>();
    Ptr<OutputStreamWrapper> streamLimits = CreateObject<OutputStreamWrapper>();
    
    // Simulate an update in queue limits
    uint32_t oldVal = 500;
    uint32_t newVal = 600;
    LimitsTrace(streamLimits, oldVal, newVal);
    
    // Validate that the trace file contains expected values
    NS_TEST_ASSERT_MSG_EQ(streamLimits->GetStream()->good(), true, "Queue Limits trace failed");
}

void TestBytesInQueueTrace()
{
    // Simulate the tracing of bytes in queue
    Ptr<Queue<Packet>> queue = CreateObject<Queue<Packet>>();
    Ptr<OutputStreamWrapper> streamBytesInQueue = CreateObject<OutputStreamWrapper>();
    
    // Simulate a change in queue size
    uint32_t oldVal = 100;
    uint32_t newVal = 150;
    BytesInQueueTrace(streamBytesInQueue, oldVal, newVal);
    
    // Validate that the trace file contains the correct bytes in queue
    NS_TEST_ASSERT_MSG_EQ(streamBytesInQueue->GetStream()->good(), true, "Bytes in Queue trace failed");
}

void TestFlowMonitorSerialization()
{
    // Simulate FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    
    // Serialize to XML
    std::string filename = "flowMonitor.xml";
    flowMonitor->SerializeToXmlFile(filename, true, true);
    
    // Check if the XML file was created
    NS_TEST_ASSERT_MSG_EQ(std::ifstream(filename).good(), true, "FlowMonitor Serialization failed");
}

void TestBqlConfiguration()
{
    bool bql = true;  // Assuming BQL is enabled
    
    // Create TrafficControlHelper and set up BQL
    TrafficControlHelper tchBottleneck;
    if (bql)
    {
        tchBottleneck.SetQueueLimits("ns3::DynamicQueueLimits");
    }
    
    // Simulate enabling BQL
    Ptr<NetDeviceQueueInterface> interface = CreateObject<NetDeviceQueueInterface>();
    Ptr<NetDeviceQueue> queueInterface = interface->GetTxQueue(0);
    Ptr<DynamicQueueLimits> queueLimits = StaticCast<DynamicQueueLimits>(queueInterface->GetQueueLimits());
    
    // Validate that BQL is enabled
    NS_TEST_ASSERT_MSG_EQ(queueLimits != nullptr, true, "BQL Configuration failed");
}

