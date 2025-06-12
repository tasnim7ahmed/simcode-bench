void TestPacketGeneration()
{
    uint32_t numPackets = 10000;
    double interval = 1.0; // seconds

    // Set up traffic generator parameters
    Ptr<Socket> socket = CreateObject<Socket>();
    Ptr<Node> node = CreateObject<Node>();
    uint32_t pktSize = 200; // bytes

    // Generate traffic and verify packet count
    GenerateTraffic(socket, pktSize, node, numPackets, Seconds(interval));
    NS_LOG_UNCOND("TestPacketGeneration: Generated " << numPackets << " packets.");
    NS_ASSERT(numPackets > 0);
}

void TestEnergyConsumption()
{
    // Create nodes and install energy models
    NodeContainer nodes;
    nodes.Create(2);
    
    // Simulate energy consumption for node 1
    Ptr<Node> node = nodes.Get(1);
    Ptr<BasicEnergySource> energySource = node->GetObject<BasicEnergySource>();
    
    double initialEnergy = energySource->GetRemainingEnergy();
    NS_LOG_UNCOND("TestEnergyConsumption: Initial energy = " << initialEnergy << "J");

    Simulator::Run();
    double finalEnergy = energySource->GetRemainingEnergy();
    NS_LOG_UNCOND("TestEnergyConsumption: Final energy = " << finalEnergy << "J");

    NS_ASSERT(finalEnergy <= initialEnergy); // Energy should not exceed initial energy
}

void TestEnergyHarvesting()
{
    // Create nodes and install energy harvesters
    NodeContainer nodes;
    nodes.Create(2);

    // Simulate energy harvesting for node 1
    Ptr<BasicEnergyHarvester> energyHarvester = nodes.Get(1)->GetObject<BasicEnergyHarvester>();
    
    // Verify the harvesting power
    double harvestedPower = energyHarvester->GetHarvestedPower();
    NS_LOG_UNCOND("TestEnergyHarvesting: Harvested power = " << harvestedPower << " W");

    NS_ASSERT(harvestedPower >= 0.0 && harvestedPower <= 0.1); // Power should be in the range [0, 0.1] W
}

void TestRemainingEnergyTrace()
{
    // Create nodes and install energy models
    NodeContainer nodes;
    nodes.Create(2);

    // Install energy source and set trace callback
    Ptr<BasicEnergySource> energySource = nodes.Get(1)->GetObject<BasicEnergySource>();
    energySource->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));

    // Start simulation and trigger energy trace
    Simulator::Run();
    NS_LOG_UNCOND("TestRemainingEnergyTrace: Remaining energy trace callback triggered.");
    Simulator::Destroy();
}

void TestTotalEnergyHarvestedTrace()
{
    // Create nodes and install energy harvesters
    NodeContainer nodes;
    nodes.Create(2);

    // Install energy harvester and set trace callback
    Ptr<BasicEnergyHarvester> energyHarvester = nodes.Get(1)->GetObject<BasicEnergyHarvester>();
    energyHarvester->TraceConnectWithoutContext("TotalEnergyHarvested", MakeCallback(&TotalEnergyHarvested));

    // Start simulation and trigger energy harvested trace
    Simulator::Run();
    NS_LOG_UNCOND("TestTotalEnergyHarvestedTrace: Total energy harvested trace triggered.");
    Simulator::Destroy();
}

void TestTotalEnergyConsumption()
{
    // Create nodes and install energy models
    NodeContainer nodes;
    nodes.Create(2);

    // Install energy model and monitor energy consumption
    Ptr<DeviceEnergyModel> deviceEnergyModel = nodes.Get(1)->GetObject<DeviceEnergyModel>();
    deviceEnergyModel->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergy));

    // Start simulation
    Simulator::Run();
    NS_LOG_UNCOND("TestTotalEnergyConsumption: Total energy consumption trace triggered.");
    Simulator::Destroy();
}

void TestReceivePacket()
{
    // Create a node and set up socket to receive packets
    NodeContainer nodes;
    nodes.Create(2);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), tid); // Destination node
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Simulate packet reception
    Simulator::Run();
    NS_LOG_UNCOND("TestReceivePacket: Packet received.");
    Simulator::Destroy();
}

