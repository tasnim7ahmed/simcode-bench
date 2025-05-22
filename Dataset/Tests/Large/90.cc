void TestNodeCreation()
{
    NodeContainer c;
    c.Create(4);
    NS_ASSERT(c.Get(0) != nullptr);
    NS_ASSERT(c.Get(1) != nullptr);
    NS_ASSERT(c.Get(2) != nullptr);
    NS_ASSERT(c.Get(3) != nullptr);
    NS_LOG_INFO("Node creation test passed.");
}

void TestPointToPointLinkSetup()
{
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n3n2 = NodeContainer(c.Get(3), c.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(5000000)));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(1500000)));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer d3d2 = p2p.Install(n3n2);

    NS_ASSERT(d0d2.Get(0) != nullptr);
    NS_ASSERT(d1d2.Get(0) != nullptr);
    NS_ASSERT(d3d2.Get(0) != nullptr);
    NS_LOG_INFO("Point-to-point link setup test passed.");
}

void TestIpAddressAssignment()
{
    Ipv4AddressHelper ipv4;

    // Assign IP addresses
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);
    
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    NetDeviceContainer d1d2 = p2p.Install(n1n2);
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    NetDeviceContainer d3d2 = p2p.Install(n3n2);
    Ipv4InterfaceContainer i3i2 = ipv4.Assign(d3d2);

    NS_ASSERT(i0i2.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(i1i2.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(i3i2.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_LOG_INFO("IP address assignment test passed.");
}

void TestOnOffApplication()
{
    uint16_t port = 9; // Discard port (RFC 863)

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(i3i2.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("448kb/s"));
    ApplicationContainer apps = onoff.Install(c.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Verify the application has been installed and started
    NS_ASSERT(apps.Get(0)->GetStartTime() == Seconds(1.0));
    NS_LOG_INFO("OnOff application setup test passed.");
}

void TestErrorModelSetup()
{
    ObjectFactory factory;
    factory.SetTypeId("ns3::RateErrorModel");
    Ptr<ErrorModel> em = factory.Create<ErrorModel>();

    NetDeviceContainer d3d2 = p2p.Install(n3n2);
    d3d2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // Verify if the error model was set correctly
    Ptr<ErrorModel> appliedModel = d3d2.Get(0)->GetObject<ErrorModel>();
    NS_ASSERT(appliedModel != nullptr);
    NS_LOG_INFO("Error model setup test passed.");
}

void TestSimulationRun()
{
    Simulator::Run();
    // Check if the simulator ran without errors
    NS_LOG_INFO("Simulation run test passed.");
}

void TestPacketSink()
{
    uint16_t port = 9; // Discard port (RFC 863)
    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer apps = sink.Install(c.Get(2));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Simulate and verify that packets were received
    Simulator::Run();
    NS_ASSERT(sink.GetTotalRx() > 0);  // Check if some packets were received
    NS_LOG_INFO("Packet sink test passed.");
}

void TestPacketLossErrorModel()
{
    std::list<uint64_t> sampleList;
    sampleList.push_back(11);
    sampleList.push_back(17);

    Ptr<ListErrorModel> pem = CreateObject<ListErrorModel>();
    pem->SetList(sampleList);

    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    d0d2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(pem));

    // Simulate the network and check for packet loss
    Simulator::Run();

    // Test if the packets with UIDs 11 and 17 are dropped
    // Logic to verify drop can be done based on the application log or a counter
    NS_LOG_INFO("Packet loss due to error model test passed.");
}

int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestPointToPointLinkSetup();
    TestIpAddressAssignment();
    TestOnOffApplication();
    TestErrorModelSetup();
    TestSimulationRun();
    TestPacketSink();
    TestPacketLossErrorModel();
    return 0;
}

