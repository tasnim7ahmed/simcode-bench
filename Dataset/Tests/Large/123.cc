void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(4);

    // Verify number of nodes
    NS_ASSERT(nodes.GetN() == 4);  // We expect four nodes to be created
}

void TestPointToPointLinks()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC, devicesCD, devicesDA;
    devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Link between node 0 and node 1
    devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2)); // Link between node 1 and node 2
    devicesCD = pointToPoint.Install(nodes.Get(2), nodes.Get(3)); // Link between node 2 and node 3
    devicesDA = pointToPoint.Install(nodes.Get(3), nodes.Get(0)); // Link between node 3 and node 0

    // Verify device count for each link
    NS_ASSERT(devicesAB.GetN() == 2);
    NS_ASSERT(devicesBC.GetN() == 2);
    NS_ASSERT(devicesCD.GetN() == 2);
    NS_ASSERT(devicesDA.GetN() == 2);
}

void TestOspfRoutingInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    OspfHelper ospf; // Helper to set up OSPF
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospf, 10); // Add OSPF with priority 10

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting); // Set the routing protocol to OSPF
    internet.Install(nodes);

    // Verify that OSPF routing is installed on each node
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ptr<RoutingProtocol> routing = ipv4->GetRoutingProtocol();
        NS_ASSERT(routing != nullptr); // Ensure the routing protocol is installed
    }
}

void TestIpAssignment()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC, devicesCD, devicesDA;
    devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Link between node 0 and node 1
    devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2)); // Link between node 1 and node 2
    devicesCD = pointToPoint.Install(nodes.Get(2), nodes.Get(3)); // Link between node 2 and node 3
    devicesDA = pointToPoint.Install(nodes.Get(3), nodes.Get(0)); // Link between node 3 and node 0

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ipv4.Assign(devicesAB);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = ipv4.Assign(devicesBC);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCD = ipv4.Assign(devicesCD);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesDA = ipv4.Assign(devicesDA);

    // Verify that each node has an IP address assigned
    NS_ASSERT(interfacesAB.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(interfacesBC.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(interfacesCD.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(interfacesDA.GetAddress(0) != Ipv4Address("0.0.0.0"));
}

void TestUdpApplications()
{
    NodeContainer nodes;
    nodes.Create(4);

    uint16_t port = 9; // Port number for the UDP application

    // UDP Server on node 3
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on node 0
    UdpClientHelper udpClient(nodes.Get(2)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
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
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC, devicesCD, devicesDA;
    devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Link between node 0 and node 1
    devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2)); // Link between node 1 and node 2
    devicesCD = pointToPoint.Install(nodes.Get(2), nodes.Get(3)); // Link between node 2 and node 3
    devicesDA = pointToPoint.Install(nodes.Get(3), nodes.Get(0)); // Link between node 3 and node 0

    pointToPoint.EnablePcapAll("ospf-routing-example");

    // Verify that the packet capture file exists
    NS_ASSERT(system("ls ospf-routing-example-0-0.pcap") == 0);  // Ensure the capture file exists
}

void TestSimulationRun()
{
    NodeContainer nodes;
    nodes.Create(4);

    // Run the simulation
    Simulator::Run();

    // Ensure the simulation runs for the expected duration (from 1.0 to 10.0 seconds)
    NS_ASSERT(Simulator::Now().GetSeconds() >= 10.0);

    Simulator::Destroy();
}

void TestSimulationCleanUp()
{
    NodeContainer nodes;
    nodes.Create(4);

    // Run the simulation
    Simulator::Run();

    // Verify that simulation is properly cleaned up
    NS_ASSERT(Simulator::GetRemainingTime() == Time(0));  // No time left after running
    Simulator::Destroy();
}

