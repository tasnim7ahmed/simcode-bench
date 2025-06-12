void TestIpAddressAssignment(Ptr<Node> node, Ipv6Address expectedAddress)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    bool addressFound = false;

    for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
        {
            if (ipv6->GetAddress(i, j).GetAddress() == expectedAddress)
            {
                addressFound = true;
                break;
            }
        }
        if (addressFound)
        {
            break;
        }
    }

    NS_ASSERT(addressFound);  // Test will fail if the expected address is not found
}

void TestRoutingTableUpdate(Ptr<Node> node, Ipv6Address expectedRoute)
{
    Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper().GetStaticRouting(node->GetObject<Ipv6>());
    std::vector<Ipv6RoutingTableEntry> routes = staticRouting->GetRoutes();

    bool routeFound = false;
    for (auto& route : routes)
    {
        if (route.GetDestNetwork() == expectedRoute)
        {
            routeFound = true;
            break;
        }
    }

    NS_ASSERT(routeFound);  // Test will fail if the expected route is not found
}

void TestRaReceptionAndIpAssignment(Ptr<Node> node, Ipv6Address expectedAddress)
{
    // Simulate RA reception
    Simulator::Schedule(Seconds(2.0), &TestIpAddressAssignment, node, expectedAddress);

    // Start the simulation and verify address assignment
    Simulator::Run();
    Simulator::Destroy();
}

void TestPingApplication(Ptr<Node> sourceNode, Ptr<Node> destinationNode)
{
    Ipv6Address destinationIp = destinationNode->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(); // Assuming the address is at index 1, 0

    PingHelper ping(destinationIp);
    ping.SetAttribute("Count", UintegerValue(5));
    ping.SetAttribute("Size", UintegerValue(1024));

    ApplicationContainer apps = ping.Install(sourceNode);
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(7.0));

    // Wait for the Ping to finish and verify the results
    Simulator::Run();
    Simulator::Destroy();
}

void TestRoutingTableOutput(Ptr<Node> node, double time)
{
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(time), node, routingStream);
}

int main(int argc, char **argv)
{
    // Setup simulation environment (same as in your original code)
    // ...

    // Initialize nodes and applications
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    // Example Test 1: Check if IP address is correctly assigned to n0
    TestIpAddressAssignment(n0, Ipv6Address("2001:1::0"));

    // Example Test 2: Verify routing table after RA reception
    TestRoutingTableUpdate(n0, Ipv6Address("2001:ABCD::"));

    // Example Test 3: Verify RA reception and IP address assignment
    TestRaReceptionAndIpAssignment(n0, Ipv6Address("2001:ABCD::0"));

    // Example Test 4: Verify ping functionality
    TestPingApplication(n0, n1);

    // Example Test 5: Print routing table at a given time
    TestRoutingTableOutput(n0, 2.0);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

