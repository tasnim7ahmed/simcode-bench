void TestCwndTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";

    // Set up the file stream to capture output
    std::string cwndFileName = "TestCwndTracer";
    uint32_t nodeId = 1;
    TraceCwnd(cwndFileName, nodeId);

    // Simulate a change in the congestion window
    CwndTracer(context, 1000, 2000);  // Old value: 1000, New value: 2000

    // Check the content of the output file or the stream for correctness
    std::ifstream file(cwndFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("0.0 1000") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "CwndTracer did not log the correct output");
}

void TestSsThreshTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold";

    // Set up the file stream to capture output
    std::string ssThreshFileName = "TestSsThreshTracer";
    uint32_t nodeId = 1;
    TraceSsThresh(ssThreshFileName, nodeId);

    // Simulate a change in the slow start threshold
    SsThreshTracer(context, 1000, 1500);  // Old value: 1000, New value: 1500

    // Check the content of the output file or the stream for correctness
    std::ifstream file(ssThreshFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("0.0 1000") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "SsThreshTracer did not log the correct output");
}

void TestRttTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTT";

    // Set up the file stream to capture output
    std::string rttFileName = "TestRttTracer";
    uint32_t nodeId = 1;
    TraceRtt(rttFileName, nodeId);

    // Simulate a change in RTT
    RttTracer(context, Time(0.01), Time(0.02));  // Old RTT: 0.01s, New RTT: 0.02s

    // Check the content of the output file or the stream for correctness
    std::ifstream file(rttFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("0.0 0.01") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "RttTracer did not log the correct output");
}

void TestRtoTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTO";

    // Set up the file stream to capture output
    std::string rtoFileName = "TestRtoTracer";
    uint32_t nodeId = 1;
    TraceRto(rtoFileName, nodeId);

    // Simulate a change in RTO
    RtoTracer(context, Time(0.05), Time(0.06));  // Old RTO: 0.05s, New RTO: 0.06s

    // Check the content of the output file or the stream for correctness
    std::ifstream file(rtoFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("0.0 0.05") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "RtoTracer did not log the correct output");
}

void TestNextTxTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/NextTxSequence";

    // Set up the file stream to capture output
    std::string nextTxFileName = "TestNextTxTracer";
    uint32_t nodeId = 1;
    TraceNextTx(nextTxFileName, nodeId);

    // Simulate a change in the next TX sequence number
    NextTxTracer(context, SequenceNumber32(1000));

    // Check the content of the output file or the stream for correctness
    std::ifstream file(nextTxFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("1000") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "NextTxTracer did not log the correct output");
}

void TestInFlightTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight";

    // Set up the file stream to capture output
    std::string inFlightFileName = "TestInFlightTracer";
    uint32_t nodeId = 1;
    TraceInFlight(inFlightFileName, nodeId);

    // Simulate a change in in-flight value
    InFlightTracer(context, 0, 1000);  // Old value: 0, New value: 1000

    // Check the content of the output file or the stream for correctness
    std::ifstream file(inFlightFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("1000") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "InFlightTracer did not log the correct output");
}

void TestNextRxTracer()
{
    // Create mock context with nodeId 1
    std::string context = "/NodeList/1/$ns3::TcpL4Protocol/SocketList/1/RxBuffer/NextRxSequence";

    // Set up the file stream to capture output
    std::string nextRxFileName = "TestNextRxTracer";
    uint32_t nodeId = 1;
    TraceNextRx(nextRxFileName, nodeId);

    // Simulate a change in the next RX sequence number
    NextRxTracer(context, SequenceNumber32(500));

    // Check the content of the output file or the stream for correctness
    std::ifstream file(nextRxFileName + ".data");
    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find("500") != std::string::npos)
        {
            found = true;
            break;
        }
    }

    // Assert that the trace output is correct
    NS_TEST_ASSERT_MSG_EQ(found, true, "NextRxTracer did not log the correct output");
}

