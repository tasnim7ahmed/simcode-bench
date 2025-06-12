#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUnitTests");

// Test 1: Node creation for star topology
void TestNodeCreation()
{
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    NS_TEST_ASSERT_MSG_EQ(centralNode.GetN(), 1, "Central node creation failed.");
    NS_TEST_ASSERT_MSG_EQ(peripheralNodes.GetN(), 4, "Peripheral nodes creation failed.");
}

// Test 2: Point-to-point link installation
void TestPointToPointInstallation()
{
    NodeContainer centralNode, peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i)
    {
        devices[i] = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(i));
    }

    for (int i = 0; i < 4; ++i)
    {
        NS_TEST_ASSERT_MSG_EQ(devices[i].GetN(), 2, "Point-to-point link installation failed for device.");
    }
}

// Test 3: IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer centralNode, peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i)
    {
        devices[i] = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    for (int i = 0; i < 4; ++i)
    {
        NS_TEST_ASSERT_MSG_NE(interfaces[i].GetAddress(1), Ipv4Address::GetAny(), "IP address assignment failed.");
    }
}

// Test 4: Application setup (UDP Echo Server/Client)
void TestApplicationSetup()
{
    NodeContainer centralNode, peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Setting up IP addresses and point-to-point links (as in main)
    PointToPointHelper pointToPoint;
    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];
    Ipv4AddressHelper address;

    for (int i = 0; i < 4; ++i)
    {
        devices[i] = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(i));
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    uint16_t port = 9; // Echo server port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(centralNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    ApplicationContainer clientApps[4];
    for (int i = 0; i < 4; ++i)
    {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        clientApps[i] = echoClient.Install(peripheralNodes.Get(i));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(10.0));
    }

    NS_TEST_ASSERT_MSG_EQ(serverApps.Get(0)->GetStartTime().GetSeconds(), 1.0, "Echo server did not start at the correct time.");
    for (int i = 0; i < 4; ++i)
    {
        NS_TEST_ASSERT_MSG_EQ(clientApps[i].Get(0)->GetStartTime().GetSeconds(), 2.0, "Echo client did not start at the correct time.");
    }
}

// Test 5: FlowMonitor statistics
void TestFlowMonitor()
{
    NodeContainer centralNode, peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];
    Ipv4AddressHelper address;

    for (int i = 0; i < 4; ++i)
    {
        devices[i] = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(i));
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Setting up applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(centralNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    ApplicationContainer clientApps[4];
    for (int i = 0; i < 4; ++i)
    {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(1), 9);
        clientApps[i] = echoClient.Install(peripheralNodes.Get(i));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(10.0));
    }

    // Install FlowMonitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // FlowMonitor statistics validation
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_NE(stats.size(), 0, "FlowMonitor did not capture any traffic");

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        NS_TEST_ASSERT_MSG_GT(it->second.txPackets, 0, "No packets transmitted.");
        NS_TEST_ASSERT_MSG_GT(it->second.rxPackets, 0, "No packets received.");
        NS_TEST_ASSERT_MSG_LT(it->second.delaySum.GetSeconds(), 1.0, "End-to-End delay too high.");
    }

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all the tests
    TestNodeCreation();
    TestPointToPointInstallation();
    TestIpAddressAssignment();
    TestApplicationSetup();
    TestFlowMonitor();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}

