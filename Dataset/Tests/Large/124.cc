void TestNodeCreation()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    // Verify the number of nodes created
    NS_ASSERT(hubNode.GetN() == 1);  // One hub node
    NS_ASSERT(clientNodes.GetN() == 3);  // Three client nodes
}

void TestCsmaNetworkSetup()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NodeContainer allNodes = NodeContainer(hubNode, clientNodes);
    NetDeviceContainer devices = csma.Install(allNodes);

    // Verify device count
    NS_ASSERT(devices.GetN() == 4);  // One hub node and three client nodes
}

void TestIpAssignment()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NodeContainer allNodes = NodeContainer(hubNode, clientNodes);
    NetDeviceContainer devices = csma.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = ipv4.Assign(devices.Get(0)); // Hub side
    vlan1Interfaces.Add(ipv4.Assign(devices.Get(1)));                     // Client 1

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = ipv4.Assign(devices.Get(2)); // Client 2

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = ipv4.Assign(devices.Get(3)); // Client 3

    // Verify that IP addresses are assigned
    NS_ASSERT(vlan1Interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(vlan2Interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"));
    NS_ASSERT(vlan3Interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"));
}

void TestRoutingSetup()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NodeContainer allNodes = NodeContainer(hubNode, clientNodes);
    NetDeviceContainer devices = csma.Install(allNodes);

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = ipv4.Assign(devices.Get(0)); // Hub side
    vlan1Interfaces.Add(ipv4.Assign(devices.Get(1)));                     // Client 1

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = ipv4.Assign(devices.Get(2)); // Client 2

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = ipv4.Assign(devices.Get(3)); // Client 3

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Verify that routing tables are populated
    for (uint32_t i = 0; i < allNodes.GetN(); i++)
    {
        Ptr<Ipv4> ipv4 = allNodes.Get(i)->GetObject<Ipv4>();
        NS_ASSERT(ipv4->GetRoutingProtocol() != nullptr);  // Ensure routing protocol is installed
    }
}

void TestUdpApplications()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    uint16_t port = 9; // UDP Echo port

    // UDP Echo Server on client node 1 (Client 1)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(clientNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on client node 2 (Client 2), sending to Client 1
    UdpEchoClientHelper echoClient(clientNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(clientNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify that server and client applications are installed
    NS_ASSERT(serverApp.GetN() == 1);
    NS_ASSERT(clientApp.GetN() == 1);
}

void TestPacketTracing()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NodeContainer allNodes = NodeContainer(hubNode, clientNodes);
    NetDeviceContainer devices = csma.Install(allNodes);

    csma.EnablePcap("trunk-port-simulation", devices);

    // Verify that the pcap file is generated
    NS_ASSERT(system("ls trunk-port-simulation-0-0.pcap") == 0);  // Ensure the capture file exists
}

void TestSimulationExecution()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    // Run the simulation
    Simulator::Run();

    // Ensure the simulation runs for the expected duration (from 1.0 to 10.0 seconds)
    NS_ASSERT(Simulator::Now().GetSeconds() >= 10.0);

    Simulator::Destroy();
}

void TestSimulationCleanUp()
{
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    // Run the simulation
    Simulator::Run();

    // Ensure the simulation is properly cleaned up
    NS_ASSERT(Simulator::GetRemainingTime() == Time(0));  // No time left after running
    Simulator::Destroy();
}

