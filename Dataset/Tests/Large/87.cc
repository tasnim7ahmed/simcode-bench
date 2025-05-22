void TestNodeCreationAndNetworkSetup() {
    NS_LOG_INFO("Test: Node creation and network stack installation");

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Check if the number of nodes created is 2
    NS_ASSERT_EQUAL(nodes.GetN(), 2);

    // Install WiFi and Internet stack
    WifiHelper wifi;
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    NetDeviceContainer nodeDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Verify if the devices are installed correctly
    NS_ASSERT_EQUAL(nodeDevices.GetN(), 2);
}

void TestWiFiChannelSetup() {
    NS_LOG_INFO("Test: WiFi channel setup");

    // Setup WiFi PHY and Channel
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Check if channel is created (non-null)
    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    NS_ASSERT(channel != nullptr);
}

void TestMobilitySetup() {
    NS_LOG_INFO("Test: Mobility setup");

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Position for node 0
    positionAlloc->Add(Vector(0.0, 50.0, 0.0)); // Position for node 1
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);

    // Check positions of nodes
    Ptr<Node> node0 = nodes.Get(0);
    Ptr<Node> node1 = nodes.Get(1);

    Vector pos0 = node0->GetObject<MobilityModel>()->GetPosition();
    Vector pos1 = node1->GetObject<MobilityModel>()->GetPosition();

    NS_ASSERT_EQUAL(pos0.x, 0.0);
    NS_ASSERT_EQUAL(pos0.y, 0.0);
    NS_ASSERT_EQUAL(pos1.x, 0.0);
    NS_ASSERT_EQUAL(pos1.y, 50.0);
}

void TestSenderReceiverSetup() {
    NS_LOG_INFO("Test: Sender and Receiver setup");

    // Create nodes and install applications
    NodeContainer nodes;
    nodes.Create(2);

    Ptr<Node> senderNode = nodes.Get(0);
    Ptr<Node> receiverNode = nodes.Get(1);

    Ptr<Sender> sender = CreateObject<Sender>();
    senderNode->AddApplication(sender);
    sender->SetStartTime(Seconds(1));

    Ptr<Receiver> receiver = CreateObject<Receiver>();
    receiverNode->AddApplication(receiver);
    receiver->SetStartTime(Seconds(0));

    // Check that applications are installed
    Ptr<Application> senderApp = senderNode->GetApplication(0);
    Ptr<Application> receiverApp = receiverNode->GetApplication(0);
    
    NS_ASSERT(senderApp != nullptr);
    NS_ASSERT(receiverApp != nullptr);
}

void TestDataCollection() {
    NS_LOG_INFO("Test: Data collection and counter");

    // Create counter calculator
    Ptr<CounterCalculator<uint32_t>> txCounter = CreateObject<CounterCalculator<uint32_t>>();
    txCounter->SetKey("wifi-tx-frames");
    txCounter->SetContext("node[0]");

    // Simulate packet transmission (this would be triggered by real events in the simulation)
    txCounter->Update();

    // Check if the counter was updated
    NS_ASSERT_EQUAL(txCounter->GetValue(), 1);
}

void TestSimulationRun() {
    NS_LOG_INFO("Test: Simulation run");

    // Create and setup nodes and network components (reuse from previous tests)
    NodeContainer nodes;
    nodes.Create(2);
    WifiHelper wifi;
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    NetDeviceContainer nodeDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Run the simulation
    Simulator::Run();

    // Ensure the simulation ran without errors
    NS_LOG_INFO("Simulation ran successfully");

    // Clean up the simulator
    Simulator::Destroy();
}

int main(int argc, char* argv[]) {
    TestNodeCreationAndNetworkSetup();
    TestWiFiChannelSetup();
    TestMobilitySetup();
    TestSenderReceiverSetup();
    TestDataCollection();
    TestSimulationRun();
    return 0;
}
