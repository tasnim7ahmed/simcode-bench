#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Test for the creation of nodes (core switch, aggregation switches, edge switches, and servers)
void TestNodeCreation()
{
    NodeContainer coreSwitch;
    coreSwitch.Create(1); // 1 core switch

    NodeContainer aggregationSwitches;
    aggregationSwitches.Create(2); // 2 aggregation switches

    NodeContainer edgeSwitches;
    edgeSwitches.Create(2); // 2 edge switches

    NodeContainer servers;
    servers.Create(4); // 4 servers

    NS_TEST_ASSERT_MSG_EQ(coreSwitch.GetN(), 1, "Core switch creation failed");
    NS_TEST_ASSERT_MSG_EQ(aggregationSwitches.GetN(), 2, "Aggregation switches creation failed");
    NS_TEST_ASSERT_MSG_EQ(edgeSwitches.GetN(), 2, "Edge switches creation failed");
    NS_TEST_ASSERT_MSG_EQ(servers.GetN(), 4, "Servers creation failed");
}

// Test for the installation of point-to-point links between switches and servers
void TestPointToPointLinks()
{
    NodeContainer coreSwitch;
    coreSwitch.Create(1); // 1 core switch

    NodeContainer aggregationSwitches;
    aggregationSwitches.Create(2); // 2 aggregation switches

    NodeContainer edgeSwitches;
    edgeSwitches.Create(2); // 2 edge switches

    NodeContainer servers;
    servers.Create(4); // 4 servers

    // Point-to-Point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect core to aggregation switches
    NetDeviceContainer coreToAggDevices;
    coreToAggDevices.Add(pointToPoint.Install(coreSwitch.Get(0), aggregationSwitches.Get(0)));
    coreToAggDevices.Add(pointToPoint.Install(coreSwitch.Get(0), aggregationSwitches.Get(1)));

    // Connect aggregation switches to edge switches
    NetDeviceContainer aggToEdgeDevices;
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(0), edgeSwitches.Get(0)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(0), edgeSwitches.Get(1)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(1), edgeSwitches.Get(0)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(1), edgeSwitches.Get(1)));

    // Connect edge switches to servers
    NetDeviceContainer edgeToServerDevices;
    for (uint32_t i = 0; i < 4 / 2; ++i)
    {
        edgeToServerDevices.Add(pointToPoint.Install(edgeSwitches.Get(0), servers.Get(i)));
        edgeToServerDevices.Add(pointToPoint.Install(edgeSwitches.Get(1), servers.Get(i + 2)));
    }

    NS_TEST_ASSERT_MSG_EQ(coreToAggDevices.GetN(), 2, "Failed to create core to aggregation switch links");
    NS_TEST_ASSERT_MSG_EQ(aggToEdgeDevices.GetN(), 4, "Failed to create aggregation to edge switch links");
    NS_TEST_ASSERT_MSG_EQ(edgeToServerDevices.GetN(), 4, "Failed to create edge to server links");
}

// Test for the installation of the Internet stack on all nodes
void TestInternetStackInstallation()
{
    NodeContainer coreSwitch;
    coreSwitch.Create(1); // 1 core switch

    NodeContainer aggregationSwitches;
    aggregationSwitches.Create(2); // 2 aggregation switches

    NodeContainer edgeSwitches;
    edgeSwitches.Create(2); // 2 edge switches

    NodeContainer servers;
    servers.Create(4); // 4 servers

    InternetStackHelper stack;
    stack.Install(coreSwitch);
    stack.Install(aggregationSwitches);
    stack.Install(edgeSwitches);
    stack.Install(servers);

    Ptr<Ipv4> ipv4 = coreSwitch.Get(0)->GetObject<Ipv4>();
    NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on core switch");

    for (uint32_t i = 0; i < aggregationSwitches.GetN(); ++i)
    {
        ipv4 = aggregationSwitches.Get(i)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on aggregation switch");
    }

    for (uint32_t i = 0; i < edgeSwitches.GetN(); ++i)
    {
        ipv4 = edgeSwitches.Get(i)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on edge switch");
    }

    for (uint32_t i = 0; i < servers.GetN(); ++i)
    {
        ipv4 = servers.Get(i)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on server");
    }
}

// Test for the assignment of IP addresses to the nodes
void TestIpAddressAssignment()
{
    NodeContainer coreSwitch;
    coreSwitch.Create(1); // 1 core switch

    NodeContainer aggregationSwitches;
    aggregationSwitches.Create(2); // 2 aggregation switches

    NodeContainer edgeSwitches;
    edgeSwitches.Create(2); // 2 edge switches

    NodeContainer servers;
    servers.Create(4); // 4 servers

    // Point-to-Point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect devices
    NetDeviceContainer coreToAggDevices;
    coreToAggDevices.Add(pointToPoint.Install(coreSwitch.Get(0), aggregationSwitches.Get(0)));
    coreToAggDevices.Add(pointToPoint.Install(coreSwitch.Get(0), aggregationSwitches.Get(1)));

    NetDeviceContainer aggToEdgeDevices;
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(0), edgeSwitches.Get(0)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(0), edgeSwitches.Get(1)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(1), edgeSwitches.Get(0)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(1), edgeSwitches.Get(1)));

    NetDeviceContainer edgeToServerDevices;
    for (uint32_t i = 0; i < 4 / 2; ++i)
    {
        edgeToServerDevices.Add(pointToPoint.Install(edgeSwitches.Get(0), servers.Get(i)));
        edgeToServerDevices.Add(pointToPoint.Install(edgeSwitches.Get(1), servers.Get(i + 2)));
    }

    InternetStackHelper stack;
    stack.Install(coreSwitch);
    stack.Install(aggregationSwitches);
    stack.Install(edgeSwitches);
    stack.Install(servers);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(ipv4.Assign(coreToAggDevices));
    interfaces.Add(ipv4.Assign(aggToEdgeDevices));
    interfaces.Add(ipv4.Assign(edgeToServerDevices));

    // Verify that IP addresses have been assigned to devices
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 12, "Failed to assign IP addresses to devices");
}

// Test for the correct installation and functionality of server applications
void TestServerApplications()
{
    NodeContainer servers;
    servers.Create(4); // 4 servers

    Ipv4InterfaceContainer interfaces;

    uint16_t port = 9; // TCP port

    // Create server applications
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        Address serverAddress(InetSocketAddress(interfaces.GetAddress(i + 6), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
        serverApps.Add(packetSinkHelper.Install(servers.Get(i)));
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 4, "Failed to install server applications");
}

// Test for the correct installation and functionality of client applications
void TestClientApplications()
{
    NodeContainer servers;
    servers.Create(4); // 4 servers

    Ipv4InterfaceContainer interfaces;

    uint16_t port = 9; // TCP port

    // Create client applications to generate TCP traffic
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(i + 6), port));
        onoff.SetAttribute("DataRate", StringValue("1Gbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientApps.Add(onoff.Install(servers.Get((i + 1) % 4)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 4, "Failed to install client applications");
}

// Test for NetAnim visualization setup
void TestNetAnimSetup()
{
    NodeContainer coreSwitch;
    coreSwitch.Create(1); // 1 core switch

    NodeContainer aggregationSwitches;
    aggregationSwitches.Create(2); // 2 aggregation switches

    NodeContainer edgeSwitches;
    edgeSwitches.Create(2); // 2 edge switches

    NodeContainer servers;
    servers.Create(4); // 4 servers

    AnimationInterface anim("fat_tree_datacenter.xml");
    anim.SetConstantPosition(coreSwitch.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(aggregationSwitches.Get(0), 30.0, 30.0);
    anim.SetConstantPosition(aggregationSwitches.Get(1), 70.0, 30.0);
    anim.SetConstantPosition(edgeSwitches.Get(0), 20.0, 10.0);
    anim.SetConstantPosition(edgeSwitches.Get(1), 80.0, 10.0);
    anim.SetConstantPosition(servers.Get(0), 10.0, 0.0);
    anim.SetConstantPosition(servers.Get(1), 30.0, 0.0);
    anim.SetConstantPosition(servers.Get(2), 70.0, 0.0);
    anim.SetConstantPosition(servers.Get(3), 90.0, 0.0);

    // Check if the positions are set correctly for visualization
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(coreSwitch.Get(0)).x, 50.0, "Core switch position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(aggregationSwitches.Get(0)).x, 30.0, "Aggregation switch 1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(aggregationSwitches.Get(1)).x, 70.0, "Aggregation switch 2 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(edgeSwitches.Get(0)).x, 20.0, "Edge switch 1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(edgeSwitches.Get(1)).x, 80.0, "Edge switch 2 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(servers.Get(0)).x, 10.0, "Server 1 position incorrect");
}

int main()
{
    TestNodeCreation();
    TestPointToPointLinks();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestServerApplications();
    TestClientApplications();
    TestNetAnimSetup();

    return 0;
}
