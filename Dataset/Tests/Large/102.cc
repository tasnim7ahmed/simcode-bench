void TestNodeCreationAndDeviceInstallation()
{
    // Create nodes
    auto nodes = NodeContainer();
    nodes.Create(5);

    // Ensure correct number of nodes
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 5, "Node creation failed!");

    // Install devices
    SimpleNetDeviceHelper simplenet;
    auto devices = simplenet.Install(nodes);

    // Ensure that devices are installed correctly
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 5, "Device installation failed!");

    // Verify that devices are assigned to correct nodes
    for (size_t i = 0; i < 5; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<NetDevice> device = devices.Get(i);
        NS_TEST_ASSERT_MSG_EQ(device->GetNode(), node, "Device assignment mismatch!");
    }
}

void TestRoutingConfiguration()
{
    // Create nodes
    auto nodes = NodeContainer();
    nodes.Create(5);

    // Install Internet Stack with routing
    Ipv4ListRoutingHelper listRouting;
    Ipv4StaticRoutingHelper staticRouting;
    listRouting.Add(staticRouting, 0);

    InternetStackHelper internet;
    internet.SetIpv6StackInstall(false);
    internet.SetIpv4ArpJitter(true);
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper ipv4address;
    ipv4address.SetBase("10.0.0.0", "255.255.255.0");
    ipv4address.Assign(devices);

    // Test static route configuration
    for (auto iter = devices.Begin(); iter != devices.End(); ++iter)
    {
        Ptr<Node> node = (*iter)->GetNode();
        auto ipv4 = node->GetObject<Ipv4>();
        auto routing = staticRouting.GetStaticRouting(ipv4);
        NS_TEST_ASSERT_MSG_NE(routing, nullptr, "Static routing is not configured properly!");
    }
}

void TestMulticastFlooding()
{
    // Create nodes
    auto nodes = NodeContainer();
    nodes.Create(5);

    // Define multicast target address
    const std::string targetAddr = "239.192.100.1";

    // Setup static routes for multicast flooding
    Ipv4StaticRoutingHelper staticRouting;
    for (auto iter = nodes.Begin(); iter != nodes.End(); ++iter)
    {
        Ptr<Node> node = *iter;
        auto ipv4 = node->GetObject<Ipv4>();
        auto routing = staticRouting.GetStaticRouting(ipv4);

        if (Names::FindName(node) == "A")
        {
            routing->AddHostRouteTo(targetAddr.c_str(), ipv4->GetInterfaceForDevice(*iter), 0);
        }
        else
        {
            staticRouting.AddMulticastRoute(node,
                                            Ipv4Address::GetAny(),
                                            targetAddr.c_str(),
                                            *iter,
                                            NetDeviceContainer(*iter));
        }
    }

    // Verify that routes are configured for multicast
    for (auto iter = nodes.Begin(); iter != nodes.End(); ++iter)
    {
        Ptr<Node> node = *iter;
        auto ipv4 = node->GetObject<Ipv4>();
        auto routing = staticRouting.GetStaticRouting(ipv4);
        bool routeExists = routing->LookupMulticastRoute(targetAddr.c_str()) != nullptr;
        NS_TEST_ASSERT_MSG_EQ(routeExists, true, "Multicast route is not set up correctly!");
    }
}

void TestPacketTransmissionAndReception()
{
    // Create nodes and devices
    auto nodes = NodeContainer();
    nodes.Create(5);
    SimpleNetDeviceHelper simplenet;
    auto devices = simplenet.Install(nodes);

    // Create a UDP socket on Node A
    Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress destination = InetSocketAddress(Ipv4Address("239.192.100.1"), 9);
    sourceSocket->Connect(destination);

    // Send data
    std::string message = "Test multicast packet";
    Ptr<Packet> packet = Create<Packet>((const uint8_t*)message.c_str(), message.size());
    sourceSocket->Send(packet);

    // Check reception at the other nodes
    for (size_t i = 1; i < 5; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<PacketSink> sink = node->GetObject<PacketSink>();
        uint64_t totalRx = sink->GetTotalRx();
        NS_TEST_ASSERT_MSG_EQ(totalRx, message.size(), "Packet reception failed at node " << Names::FindName(node));
    }
}

void TestPcapTraces()
{
    // Create nodes and devices
    auto nodes = NodeContainer();
    nodes.Create(5);
    SimpleNetDeviceHelper simplenet;
    auto devices = simplenet.Install(nodes);

    // Set up PCAP tracing
    InternetStackHelper internet;
    internet.Install(nodes);
    for (auto iter = nodes.Begin(); iter != nodes.End(); ++iter)
    {
        internet.EnablePcapIpv4("smf-trace", (*iter)->GetId(), 1, false);
    }

    // Run simulation
    Simulator::Run();

    // Verify if the trace file is created
    std::ifstream traceFile("smf-trace-0-0.pcap");
    NS_TEST_ASSERT_MSG_EQ(traceFile.is_open(), true, "PCAP trace file was not generated!");

    // Cleanup
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    TestNodeCreationAndDeviceInstallation();
    TestRoutingConfiguration();
    TestMulticastFlooding();
    TestPacketTransmissionAndReception();
    TestPcapTraces();

    return 0;
}
