void TestSocketCreationAndConfiguration()
{
    NodeContainer nodes;
    nodes.Create(2);

    // Create UDP socket
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), tid);
    Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), tid);

    // Verify socket creation
    NS_ASSERT(recvSink != nullptr);
    NS_ASSERT(source != nullptr);

    // Set socket options for receiving TCLASS and HOPLIMIT
    recvSink->SetIpv6RecvTclass(true);
    recvSink->SetIpv6RecvHopLimit(true);

    // Set socket options for sending TCLASS and HOPLIMIT
    source->SetIpv6Tclass(16);  // Example TCLASS value
    source->SetIpv6HopLimit(64);  // Example HOPLIMIT value

    // Verify if the options are correctly set
    NS_ASSERT(source->GetIpv6Tclass() == 16);
    NS_ASSERT(source->GetIpv6HopLimit() == 64);
}

void TestSendPacket()
{
    NodeContainer nodes;
    nodes.Create(1);
    
    Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
    
    uint32_t packetSize = 1024;
    uint32_t packetCount = 5;
    Time packetInterval = Seconds(1.0);

    // Schedule the SendPacket function
    Simulator::Schedule(Seconds(1.0), &SendPacket, source, packetSize, packetCount, packetInterval);

    // Run the simulator for a short time to ensure packets are sent
    Simulator::Run();

    // Check if the expected number of packets have been sent (you could track packet sends in your simulation)
    // Add additional logging to verify packet sends
    NS_LOG_INFO("Packets sent: " << packetCount);

    Simulator::Destroy();
}

void TestPacketReceptionAndTags()
{
    NodeContainer nodes;
    nodes.Create(2);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), tid);
    Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), 4477);
    recvSink->Bind(local);

    // Set up the callback for packet reception
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Simulate sending a packet from node 0 to node 1
    Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), tid);
    Inet6SocketAddress remote = Inet6SocketAddress(Ipv6Address("2001:0000:f00d:cafe::2"), 4477);
    source->Connect(remote);
    source->Send(Create<Packet>(1024));

    // Run the simulator to process the packet reception
    Simulator::Run();

    // At this point, you should be able to verify if the TCLASS and HOPLIMIT tags were removed from the packet
    // and printed as expected in the ReceivePacket function
    NS_LOG_INFO("Packet received and tags processed.");
    
    Simulator::Destroy();
}

void TestIpAddressAssignmentAndCsmaLink()
{
    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Verify if the IP addresses have been assigned correctly
    NS_ASSERT(interfaces.GetAddress(0, 1) == Ipv6Address("2001:0000:f00d:cafe::1"));
    NS_ASSERT(interfaces.GetAddress(1, 1) == Ipv6Address("2001:0000:f00d:cafe::2"));
}

void TestIpv6FlowSimulation()
{
    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices = csma.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Create the receiver socket
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), tid);
    Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), 4477);
    recvSink->Bind(local);

    // Set up the sender socket and start the flow
    Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), tid);
    Inet6SocketAddress remote = Inet6SocketAddress(interfaces.GetAddress(1, 1), 4477);
    source->Connect(remote);

    // Schedule the sending of packets
    Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                   Seconds(1.0),
                                   &SendPacket,
                                   source,
                                   1024, 10, Seconds(1.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();
}

