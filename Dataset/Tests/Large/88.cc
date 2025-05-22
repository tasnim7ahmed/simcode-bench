void TestReceivePacket()
{
    // Set up nodes, devices, and other necessary configurations
    NodeContainer c;
    c.Create(2);
    WifiHelper wifi;
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> wifiChannelPtr = wifiChannel.Create();
    wifiPhy.SetChannel(wifiChannelPtr);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);
    
    // Create the socket
    Ptr<Socket> socket = Socket::CreateSocket(c.Get(1), TypeId::LookupByName("ns3::UdpSocketFactory"));
    socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 80));
    socket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Simulate packet reception
    InetSocketAddress remote = InetSocketAddress(Ipv4Address::GetBroadcast(), 80);
    Ptr<Socket> source = Socket::CreateSocket(c.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
    source->Connect(remote);
    source->Send(Create<Packet>(200));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    // Check if the packet was received correctly
    // You can check logs to confirm if the "PrintReceivedPacket" was triggered
}

void TestEnergyConsumption()
{
    NodeContainer c;
    c.Create(2);

    // Set up energy source and energy model for devices
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(0.1));
    EnergySourceContainer sources = basicSourceHelper.Install(c);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    NetDeviceContainer devices = radioEnergyHelper.Install(devices, sources);

    // Set up the trace for energy consumption
    Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource>(sources.Get(1));
    basicSourcePtr->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));

    // Run the simulation and check energy consumption
    Simulator::Run();
    for (auto iter = devices.Begin(); iter != devices.End(); iter++)
    {
        double energyConsumed = (*iter)->GetTotalEnergyConsumption();
        NS_LOG_UNCOND("Total energy consumed by radio: " << energyConsumed << "J");
        NS_ASSERT(energyConsumed <= 0.1); // Check if energy consumption is within the expected range
    }

    Simulator::Destroy();
}

void TestNodeDistanceAndPosition()
{
    NodeContainer c;
    c.Create(2);
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));  // Setting distance between nodes to 100 meters

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    // Run the simulation to check positions
    Simulator::Run();
    Ptr<ConstantPositionMobilityModel> mobModel1 = c.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mobModel2 = c.Get(1)->GetObject<ConstantPositionMobilityModel>();

    // Assert if the nodes are placed at the correct positions
    double distance = mobModel1->GetPosition().x - mobModel2->GetPosition().x;
    NS_LOG_UNCOND("Distance between nodes: " << distance);
    NS_ASSERT(distance == 100.0);  // Check if the distance between nodes is 100 meters

    Simulator::Destroy();
}

void TestGenerateTraffic()
{
    NodeContainer c;
    c.Create(2);

    Ptr<Socket> source = Socket::CreateSocket(c.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
    InetSocketAddress remote = InetSocketAddress(Ipv4Address::GetBroadcast(), 80);
    source->Connect(remote);

    // Set up traffic generation
    Simulator::Schedule(Seconds(1.0), &GenerateTraffic, source, 200, c.Get(0), 100, Seconds(1));

    // Run the simulation to check packet transmission
    Simulator::Run();
    Simulator::Destroy();

    // Check if packets were generated (you can track this in the logs or with a packet counter)
}

void TestIpAddressAssignment()
{
    NodeContainer c;
    c.Create(2);

    // Set up internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(c);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(c.Get(0)->GetDevice(0));

    // Check if IP addresses are assigned correctly
    Ipv4Address ip = interfaces.GetAddress(0);
    NS_LOG_UNCOND("Assigned IP address: " << ip);
    NS_ASSERT(ip != Ipv4Address("0.0.0.0"));  // Ensure the IP address is assigned

    Simulator::Destroy();
}

void TestEnergySourceInitialization()
{
    NodeContainer c;
    c.Create(1); // One node to test

    // Set up energy source
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(0.1));
    EnergySourceContainer sources = basicSourceHelper.Install(c);

    // Get the energy source and check the initial energy
    Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource>(sources.Get(0));
    double initialEnergy = energySource->GetInitialEnergy();
    NS_LOG_UNCOND("Initial Energy: " << initialEnergy);
    NS_ASSERT(initialEnergy == 0.1);  // Check if the initial energy is correctly set

    Simulator::Destroy();
}

