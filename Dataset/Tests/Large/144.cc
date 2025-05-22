#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyExampleTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(4); // Create 4 nodes

    // Ensure we have 4 nodes created
    NS_ASSERT_MSG(nodes.GetN() == 4, "4 nodes should be created in the network.");
}

// Test 2: Verify Point-to-Point Link Installation in Ring Topology
void TestPointToPointLinkInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    // Ensure devices are installed correctly in a ring topology
    NS_ASSERT_MSG(devices.GetN() == 8, "There should be 8 devices installed in the ring topology.");
}

// Test 3: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    // Ensure IP addresses are assigned correctly
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(interfaces.GetAddress(i) != Ipv4Address("0.0.0.0"), "Each node should have a valid IP address.");
    }
}

// Test 4: Verify OnOff Application Installation
void TestOnOffApplicationInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    uint16_t port = 9;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress((i + 1) % nodes.GetN()), port));
        onoff.SetConstantRate(DataRate("1Mbps"), 1024);
        ApplicationContainer app = onoff.Install(nodes.Get(i));
        app.Start(Seconds(1.0 + i));
        app.Stop(Seconds(10.0));
    }

    // Ensure the OnOff application is installed on each node
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(app.GetN() == 1, "The OnOff application should be installed on each node.");
    }
}

// Test 5: Verify PacketSink Application Installation
void TestPacketSinkApplicationInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    uint16_t port = 9;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(i));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
    }

    // Ensure the PacketSink application is installed on each node
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(sinkApp.GetN() == 1, "The PacketSink application should be installed on each node.");
    }
}

// Test 6: Verify Routing Tables Population
void TestRoutingTables()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Ensure routing tables are populated for each node
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        NS_ASSERT_MSG(ipv4->GetRoutingProtocol()->GetNRoutes() > 0, "Routing table for node " + std::to_string(i) + " should have entries.");
    }
}

// Test 7: Verify Data Flow from Node to Neighbor in Ring Topology
void TestDataFlow()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    uint16_t port = 9;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress((i + 1) % nodes.GetN()), port));
        onoff.SetConstantRate(DataRate("1Mbps"), 1024);
        ApplicationContainer app = onoff.Install(nodes.Get(i));
        app.Start(Seconds(1.0 + i));
        app.Stop(Seconds(10.0));
    }

    // Ensure packets are received correctly at each node's PacketSink
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(sinkApp.Get(i));
        NS_ASSERT_MSG(sinkPtr->GetTotalBytesReceived() > 0, "No packets received at node " + std::to_string(i));
    }
}

int main()
{
    TestNodeCreation();
    TestPointToPointLinkInstallation();
    TestIpAddressAssignment();
    TestOnOffApplicationInstallation();
    TestPacketSinkApplicationInstallation();
    TestRoutingTables();
    TestDataFlow();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
