void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Testing Node Creation...");
    NS_ASSERT(nodes.GetN() == 2);  // Ensure two nodes are created
}

void TestMobilityInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> nodesPositionAlloc = CreateObject<ListPositionAllocator>();
    nodesPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    nodesPositionAlloc->Add(Vector(50.0, 0.0, 0.0));
    mobility.SetPositionAllocator(nodesPositionAlloc);
    mobility.Install(nodes);

    NS_LOG_INFO("Testing Mobility Installation...");

    Ptr<MobilityModel> mobilityModelNode0 = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mobilityModelNode1 = nodes.Get(1)->GetObject<MobilityModel>();

    NS_ASSERT(mobilityModelNode0->GetPosition() == Vector(0.0, 0.0, 0.0));  // Node 0 at position (0,0,0)
    NS_ASSERT(mobilityModelNode1->GetPosition() == Vector(50.0, 0.0, 0.0)); // Node 1 at position (50,0,0)
}

void TestChannelCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devContainer = lrWpanHelper.Install(nodes);
    lrWpanHelper.CreateAssociatedPan(devContainer, 10);

    NS_LOG_INFO("Testing Channel Creation...");

    NS_ASSERT(devContainer.GetN() == 2);  // Ensure two devices are created
}

void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);  // Disable IPv4 stack
    internetv6.Install(nodes);

    NS_LOG_INFO("Testing Internet Stack Installation...");

    Ptr<Ipv6> ipv6 = nodes.Get(0)->GetObject<Ipv6>();
    NS_ASSERT(ipv6 != nullptr);  // Ensure IPv6 is installed correctly
}

void TestSixLoWPANInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devContainer = lrWpanHelper.Install(nodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer six1 = sixlowpan.Install(devContainer);

    NS_LOG_INFO("Testing 6LoWPAN Installation...");

    NS_ASSERT(six1.GetN() == 2);  // Ensure two devices have 6LoWPAN installed
}

void TestIpv6AddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devContainer = lrWpanHelper.Install(nodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer six1 = sixlowpan.Install(devContainer);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i = ipv6.Assign(six1);

    NS_LOG_INFO("Testing IPv6 Address Assignment...");

    Ipv6Address node0Address = i.GetAddress(0, 1);
    Ipv6Address node1Address = i.GetAddress(1, 1);

    NS_ASSERT(node0Address.IsValid());  // Ensure node 0 has a valid IPv6 address
    NS_ASSERT(node1Address.IsValid());  // Ensure node 1 has a valid IPv6 address
}

void TestPingApplicationSetup()
{
    NodeContainer nodes;
    nodes.Create(2);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    SixLowPanHelper sixlowpan;
    NetDeviceContainer devContainer = sixlowpan.Install(nodes);

    Ipv6InterfaceContainer i = ipv6.Assign(devContainer);

    uint32_t packetSize = 16;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);
    PingHelper ping(i.GetAddress(1, 1));

    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("Size", UintegerValue(packetSize));

    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    NS_LOG_INFO("Testing Ping Application Setup...");

    NS_ASSERT(apps.GetN() == 1);  // Ensure that one application is installed
}

void TestTraceFileGeneration()
{
    NodeContainer nodes;
    nodes.Create(2);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devContainer = lrWpanHelper.Install(nodes);

    AsciiTraceHelper ascii;
    lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("ping6wsn.tr"));
    lrWpanHelper.EnablePcapAll(std::string("ping6wsn"), true);

    NS_LOG_INFO("Testing Trace File Generation...");

    // Check if the trace file "ping6wsn.tr" is created
    std::ifstream traceFile("ping6wsn.tr");
    NS_ASSERT(traceFile.is_open());  // Ensure the trace file is created
}

int main(int argc, char** argv)
{
    // Enable verbose logging if required
    bool verbose = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("Ping6WsnExample", LOG_LEVEL_INFO);
    }

    // Run the individual tests
    TestNodeCreation();
    TestMobilityInstallation();
    TestChannelCreation();
    TestInternetStackInstallation();
    TestSixLoWPANInstallation();
    TestIpv6AddressAssignment();
    TestPingApplicationSetup();
    TestTraceFileGeneration();

    return 0;
}
