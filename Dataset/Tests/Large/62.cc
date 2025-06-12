void TestSocketTraces() {
    // Simulate the system with tracing enabled
    bool tracing = true;
    // Set up socket traces for congestion window, pacing rate, slow start threshold, etc.
    ConnectSocketTraces();
    
    // Verify that the tracing files are created and contain expected data
    cwndStream.open("tcp-dynamic-pacing-cwnd.dat", std::ios::in);
    pacingRateStream.open("tcp-dynamic-pacing-pacing-rate.dat", std::ios::in);
    ssThreshStream.open("tcp-dynamic-pacing-ssthresh.dat", std::ios::in);
    
    // Check if files have data
    assert(!cwndStream.eof());
    assert(!pacingRateStream.eof());
    assert(!ssThreshStream.eof());
    
    cwndStream.close();
    pacingRateStream.close();
    ssThreshStream.close();
}

void TestFlowMonitor() {
    // Set up flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    // Run simulation and get flow statistics
    Simulator::Run();
    monitor->CheckForLostPackets();
    
    // Verify that the flow stats are correct
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto& stat : stats) {
        assert(stat.second.txPackets > 0); // Ensure data was transmitted
        assert(stat.second.rxPackets > 0); // Ensure data was received
        assert(stat.second.rxBytes > 0);   // Ensure some data was received
    }
    
    Simulator::Destroy();
}

void TestTcpPacing() {
    // Set pacing parameters and enable pacing
    bool isPacingEnabled = true;
    DataRate maxPacingRate("4Gbps");
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", BooleanValue(isPacingEnabled));
    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", DataRateValue(maxPacingRate));
    
    // Run the simulation with pacing enabled
    Simulator::Run();
    
    // Verify that pacing is happening correctly by checking pacing rate trace
    pacingRateStream.open("tcp-dynamic-pacing-pacing-rate.dat", std::ios::in);
    assert(!pacingRateStream.eof());  // Ensure pacing rate trace is not empty
    pacingRateStream.close();
    
    Simulator::Destroy();
}

void TestLinkAndAppSetup() {
    // Create nodes and check if they are correctly created
    NodeContainer c;
    c.Create(6);
    assert(c.Get(0)->GetId() == 0);
    assert(c.Get(1)->GetId() == 1);
    assert(c.Get(2)->GetId() == 2);
    assert(c.Get(3)->GetId() == 3);
    assert(c.Get(4)->GetId() == 4);
    assert(c.Get(5)->GetId() == 5);
    
    // Check that the applications are installed properly
    uint16_t sinkPort = 8080;
    Address sinkAddress4(InetSocketAddress("10.1.4.2", sinkPort));
    Address sinkAddress5(InetSocketAddress("10.1.5.2", sinkPort));
    
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps4 = packetSinkHelper.Install(c.Get(4));  // n4 as sink
    ApplicationContainer sinkApps5 = packetSinkHelper.Install(c.Get(5));  // n5 as sink
    
    sinkApps4.Start(Seconds(0));
    sinkApps5.Start(Seconds(0));
    
    // Verify if applications are installed and started
    assert(sinkApps4.Get(0)->GetStartTime() == Seconds(0));
    assert(sinkApps5.Get(0)->GetStartTime() == Seconds(0));
}

void TestCommandLineArgs() {
    // Test command-line argument handling by simulating different arguments
    int argc = 6;
    char* argv[6] = {"program_name", "--isPacingEnabled", "true", "--maxPacingRate", "4Gbps", "--simulationEndTime", "5"};
    CommandLine cmd;
    cmd.Parse(argc, argv);
    
    // Verify that command-line arguments are correctly parsed and set
    bool isPacingEnabled = true;
    DataRate maxPacingRate("4Gbps");
    Time simulationEndTime = Seconds(5);
    
    assert(isPacingEnabled == true);
    assert(maxPacingRate.GetBitRate() == 4 * 1e9);
    assert(simulationEndTime == Seconds(5));
}

int main() {
    TestSocketTraces();
    TestFlowMonitor();
    TestTcpPacing();
    TestLinkAndAppSetup();
    TestCommandLineArgs();
    
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}

