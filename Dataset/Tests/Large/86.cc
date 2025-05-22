void TestSenderInitialization()
{
    Ptr<Sender> sender = CreateObject<Sender>();
    
    // Check if default packet size is 64
    NS_TEST_ASSERT_MSG_EQ(sender->GetPacketSize(), 64, "Packet size should be 64 bytes by default");

    // Check if default destination is 255.255.255.255
    NS_TEST_ASSERT_MSG_EQ(sender->GetDestination(), Ipv4Address("255.255.255.255"), "Default destination should be 255.255.255.255");

    // Check if default port is 1603
    NS_TEST_ASSERT_MSG_EQ(sender->GetPort(), 1603, "Default port should be 1603");
    
    // Check if default number of packets is 30
    NS_TEST_ASSERT_MSG_EQ(sender->GetNumPackets(), 30, "Default number of packets should be 30");
    
    // Check if the interval is set correctly (should be a constant random variable)
    NS_TEST_ASSERT_MSG_NE(sender->GetInterval(), nullptr, "Interval should be a valid RandomVariable");
}

void TestReceiverInitialization()
{
    Ptr<Receiver> receiver = CreateObject<Receiver>();
    
    // Check if default port is 1603
    NS_TEST_ASSERT_MSG_EQ(receiver->GetPort(), 1603, "Default port should be 1603");
}

void TestSenderPacketSending()
{
    Ptr<Sender> sender = CreateObject<Sender>();
    sender->SetPacketSize(128);
    sender->SetDestination(Ipv4Address("10.1.1.2"));
    sender->SetPort(1605);
    sender->SetNumPackets(5);

    // Start the application
    sender->StartApplication();
    
    // Run the simulator for enough time to send all packets
    Simulator::Run();
    
    // Assert that the correct number of packets were sent
    NS_TEST_ASSERT_MSG_EQ(sender->GetSentPacketCount(), 5, "Sender should send 5 packets");

    // Stop the application
    sender->StopApplication();
}

void TestReceiverPacketReception()
{
    Ptr<Receiver> receiver = CreateObject<Receiver>();
    receiver->SetPort(1605);
    
    Ptr<Sender> sender = CreateObject<Sender>();
    sender->SetPacketSize(128);
    sender->SetDestination(Ipv4Address("10.1.1.2"));
    sender->SetPort(1605);
    
    // Create the delay calculator and link it to the receiver
    Ptr<TimeMinMaxAvgTotalCalculator> delayCalculator = CreateObject<TimeMinMaxAvgTotalCalculator>();
    receiver->SetDelayTracker(delayCalculator);
    
    // Create the counter calculator and link it to the receiver
    Ptr<CounterCalculator<>> counterCalculator = CreateObject<CounterCalculator<>>();
    receiver->SetCounter(counterCalculator);
    
    // Start the receiver
    receiver->StartApplication();
    
    // Start the sender
    sender->StartApplication();

    // Run the simulator for enough time to process packets
    Simulator::Run();
    
    // Check if the receiver has received any packets and updated the delay
    NS_TEST_ASSERT_MSG_GT(delayCalculator->GetAvg(), 0, "Average delay should be greater than zero");
    NS_TEST_ASSERT_MSG_GT(counterCalculator->GetCount(), 0, "Counter should have updated after receiving packets");
    
    // Stop the applications
    sender->StopApplication();
    receiver->StopApplication();
}

void TestSenderReceiverInteraction()
{
    // Create sender and receiver objects
    Ptr<Sender> sender = CreateObject<Sender>();
    Ptr<Receiver> receiver = CreateObject<Receiver>();
    
    // Setup sender parameters
    sender->SetPacketSize(64);
    sender->SetDestination(Ipv4Address("10.1.1.2"));
    sender->SetPort(1605);
    sender->SetNumPackets(5);
    
    // Setup receiver parameters
    receiver->SetPort(1605);
    
    // Link the sender and receiver in the simulator
    NodeContainer nodes;
    nodes.Create(2);
    nodes.Get(0)->AddApplication(sender);
    nodes.Get(1)->AddApplication(receiver);

    // Start the applications
    sender->StartApplication();
    receiver->StartApplication();

    // Run the simulator for enough time to process packets
    Simulator::Run();
    
    // Check if the receiver has received the correct number of packets
    NS_TEST_ASSERT_MSG_EQ(receiver->GetReceivedPacketCount(), 5, "Receiver should receive 5 packets");

    // Stop the applications
    sender->StopApplication();
    receiver->StopApplication();
}

void TestSenderTransmissionInterval()
{
    Ptr<Sender> sender = CreateObject<Sender>();
    
    // Set a custom interval
    sender->SetInterval(Seconds(1.0)); // Interval of 1 second
    
    // Start the sender
    sender->StartApplication();

    // Run the simulator for enough time
    Simulator::Run();
    
    // Check if the first packet was sent at the expected time
    NS_TEST_ASSERT_MSG_GT(sender->GetFirstSentTime().GetSeconds(), 0, "First packet should be sent after a valid delay");

    // Stop the sender
    sender->StopApplication();
}

void TestSenderReceiverTimestamp()
{
    Ptr<Receiver> receiver = CreateObject<Receiver>();
    receiver->SetPort(1605);
    
    Ptr<Sender> sender = CreateObject<Sender>();
    sender->SetPacketSize(64);
    sender->SetDestination(Ipv4Address("10.1.1.2"));
    sender->SetPort(1605);

    // Start the applications
    sender->StartApplication();
    receiver->StartApplication();
    
    // Run the simulator for enough time to send and receive packets
    Simulator::Run();
    
    // Check if the receiver was able to extract the timestamp from the received packet
    NS_TEST_ASSERT_MSG_GT(receiver->GetReceivedTimestampCount(), 0, "Receiver should have extracted timestamps from packets");
    
    // Stop the applications
    sender->StopApplication();
    receiver->StopApplication();
}

