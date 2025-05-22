void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Verify number of nodes
    NS_ASSERT(nodes.GetN() == 2);  // We expect two nodes to be created
}

void TestPointToPointLink()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify device count (should be 2 devices: one for each node)
    NS_ASSERT(devices.GetN() == 2);

    // Verify the channel delay
    Ptr<PointToPointNetDevice> dev0 = devices.Get(0)->GetObject<PointToPointNetDevice>();
    Ptr<PointToPointChannel> channel = dev0->GetChannel()->GetObject<PointToPointChannel>();
    NS_ASSERT(channel->GetDelay() == TimeValue(Seconds(0.002)));  // Check delay is 2ms
}

void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);

    // Verify that the Internet stack has been installed
    Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_1 = nodes.Get(1)->GetObject<Ipv4>();

    NS_ASSERT(ipv4_0 != nullptr);  // Check if Ipv4 is installed on node 0
    NS_ASSERT(ipv4_1 != nullptr);  // Check if Ipv4 is installed on node 1
}

void TestIpAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify that both nodes have assigned IP addresses
    NS_ASSERT(interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"));  // Node 0 IP address
    NS_ASSERT(interfaces.GetAddress(1) != Ipv4Address("0.0.0.0"));  // Node 1 IP address
}

void TestUdpApplications()
{
    NodeContainer nodes;
    nodes.Create(2);

    uint16_t port = 9; // well-known echo port number

    // UDP Server on node 1
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on node 0
    UdpClientHelper udpClient(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Ensure server and client apps are installed
    NS_ASSERT(serverApp.GetN() == 1);
    NS_ASSERT(clientApp.GetN() == 1);
}

void TestPacketTracing()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("simple-netanim.tr"));

    // Verify that the trace file was created
    NS_ASSERT(system("ls simple-netanim.tr") == 0);  // Ensure the trace file exists
}

void TestNetAnimSetup()
{
    NodeContainer nodes;
    nodes.Create(2);

    AnimationInterface anim("simple-netanim.xml");

    // Set node positions
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);  // Node 0 at (0,0)
    anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0); // Node 1 at (10,0)

    // Verify that positions are set correctly
    Vector pos0 = anim.GetNodePosition(0); // Get position of node 0
    Vector pos1 = anim.GetNodePosition(1); // Get position of node 1

    NS_ASSERT(pos0.x == 0.0 && pos0.y == 0.0);
    NS_ASSERT(pos1.x == 10.0 && pos1.y == 0.0);
}

void TestSimulationRun()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Run simulation
    Simulator::Run();

    // Ensure the simulation runs for the expected duration (from 1.0 to 10.0 seconds)
    NS_ASSERT(Simulator::Now().GetSeconds() >= 10.0);

    Simulator::Destroy();
}

void TestSimulationCleanUp()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Run the simulation
    Simulator::Run();

    // Verify that simulation is properly cleaned up
    NS_ASSERT(Simulator::GetRemainingTime() == Time(0));  // No time left after running
    Simulator::Destroy();
}

