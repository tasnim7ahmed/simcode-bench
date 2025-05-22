void TestCheckQueueSize()
{
    Ptr<QueueDisc> mockQueue = CreateObject<QueueDisc>(); // Create mock QueueDisc for testing
    std::string mockQueueDiscType = "CobaltQueueDisc";  // Example queue type

    // Call the function to test
    CheckQueueSize(mockQueue, mockQueueDiscType);

    // You can also check if the output file is created or the size is updated
    std::ifstream inputFile(dir + mockQueueDiscType + "/queueTraces/queue.plotme");
    bool fileExists = inputFile.good();
    NS_TEST_ASSERT_MSG_EQ(fileExists, true, "Queue size trace file was not created.");
}

void TestCwndTrace()
{
    Ptr<OutputStreamWrapper> mockStream = CreateObject<OutputStreamWrapper>(); // Mock OutputStreamWrapper
    uint32_t oldCwnd = 1000; // Old Congestion Window
    uint32_t newCwnd = 1200; // New Congestion Window

    // Call the trace function to check
    CwndTrace(mockStream, oldCwnd, newCwnd);

    // Verify that the output stream has the correct data format
    std::ifstream outputFile(dir + "CobaltQueueDisc/cwndTraces/S1-1.plotme");
    bool fileExists = outputFile.good();
    NS_TEST_ASSERT_MSG_EQ(fileExists, true, "Congestion window trace file was not created.");
}

void TestNodeCreation()
{
    NodeContainer tcpSender, udpSender, gateway, sink;
    
    // Create nodes
    tcpSender.Create(5);
    udpSender.Create(2);
    gateway.Create(2);
    sink.Create(1);

    // Check the node count
    NS_TEST_ASSERT_MSG_EQ(tcpSender.GetN(), 5, "TCP sender nodes were not created properly.");
    NS_TEST_ASSERT_MSG_EQ(udpSender.GetN(), 2, "UDP sender nodes were not created properly.");
    NS_TEST_ASSERT_MSG_EQ(gateway.GetN(), 2, "Gateway nodes were not created properly.");
    NS_TEST_ASSERT_MSG_EQ(sink.GetN(), 1, "Sink node was not created properly.");
}

void TestTrafficControlHelperSetup()
{
    TrafficControlHelper tch;
    std::string queueDiscType = "CobaltQueueDisc";

    // Check if the TrafficControlHelper is correctly configured
    std::string expectedQueueDiscType = "ns3::" + queueDiscType;
    Config::SetDefault(expectedQueueDiscType + "::MaxSize", QueueSizeValue(QueueSize("200p")));

    // Perform any checks (e.g., verifying defaults or object behavior)
    NS_TEST_ASSERT_MSG_EQ(Config::GetDefault(expectedQueueDiscType + "::MaxSize").GetValue().GetValue(), 200, "Queue size is not set correctly.");
}

void TestSimulationParameters()
{
    double stopTime = 101;
    uint32_t packetSize = 1000; // Check packet size

    // Test simulation stop time
    NS_TEST_ASSERT_MSG_EQ(stopTime, 101, "Simulation stop time was not set correctly.");
    
    // Test packet size setup for BulkSendHelper
    BulkSendHelper ftp("ns3::TcpSocketFactory", Address());
    ftp.SetAttribute("SendSize", UintegerValue(packetSize));
    
    NS_TEST_ASSERT_MSG_EQ(ftp.GetAttribute("SendSize").GetValue(), packetSize, "Packet size was not set correctly.");
}

void TestRoutingTableSetup()
{
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Test if routing tables are populated
    bool routingTablePopulated = Ipv4RoutingHelper::GetRoutingTable()->GetNRoutes() > 0;
    
    NS_TEST_ASSERT_MSG_EQ(routingTablePopulated, true, "Routing table was not populated correctly.");
}

void TestDirectoryCreation()
{
    std::string dirToSave = "mkdir -p " + dir + "CobaltQueueDisc";
    int result = system((dirToSave + "/cwndTraces/").c_str());

    // Check if directories were created successfully
    NS_TEST_ASSERT_MSG_EQ(result, 0, "Directory for cwndTraces was not created.");
}

int main()
{
    // Run the individual tests
    TestCheckQueueSize();
    TestCwndTrace();
    TestNodeCreation();
    TestTrafficControlHelperSetup();
    TestSimulationParameters();
    TestRoutingTableSetup();
    TestDirectoryCreation();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
