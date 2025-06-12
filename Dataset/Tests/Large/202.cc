#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkTest");

// Test 1: Sensor node creation and device installation
void TestNodeCreationAndDeviceInstallation()
{
    // Create sensor nodes
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    NS_TEST_ASSERT_MSG_EQ(sensorNodes.GetN(), 6, "Failed to create six sensor nodes.");

    // Set up the physical layer with LR-WPAN
    LrWpanHelper lrWpan;
    NetDeviceContainer sensorDevices = lrWpan.Install(sensorNodes);
    
    NS_TEST_ASSERT_MSG_EQ(sensorDevices.GetN(), 6, "Failed to install LR-WPAN devices on sensor nodes.");

    // Set up 6LoWPAN layer
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(sensorDevices);
    
    NS_TEST_ASSERT_MSG_EQ(sixlowpanDevices.GetN(), 6, "Failed to install 6LoWPAN on sensor devices.");
}

// Test 2: IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    // Set up physical layer and 6LoWPAN
    LrWpanHelper lrWpan;
    NetDeviceContainer sensorDevices = lrWpan.Install(sensorNodes);
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(sensorDevices);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(sensorNodes);

    // Assign IP addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sensorInterfaces = ipv6.Assign(sixlowpanDevices);

    NS_TEST_ASSERT_MSG_NE(sensorInterfaces.GetAddress(0, 1), Ipv6Address::GetAny(), "Failed to assign IP address to sink node.");
    for (int i = 1; i < 6; ++i)
    {
        NS_TEST_ASSERT_MSG_NE(sensorInterfaces.GetAddress(i, 1), Ipv6Address::GetAny(), "Failed to assign IP address to sensor node.");
    }
}

// Test 3: UDP Echo application setup
void TestUdpEchoApplication()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    // Set up physical and 6LoWPAN layers
    LrWpanHelper lrWpan;
    NetDeviceContainer sensorDevices = lrWpan.Install(sensorNodes);
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(sensorDevices);

    // Install internet stack and IP addresses
    InternetStackHelper internet;
    internet.Install(sensorNodes);
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sensorInterfaces = ipv6.Assign(sixlowpanDevices);

    // Set up UDP echo server on the sink node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(sensorNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp.Get(0)->GetStartTime().GetSeconds(), 1.0, "UDP echo server start time is incorrect.");

    // Set up UDP echo clients on the peripheral sensor nodes
    UdpEchoClientHelper echoClient(sensorInterfaces.GetAddress(0, 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (int i = 1; i < 6; ++i)
    {
        clientApps.Add(echoClient.Install(sensorNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    for (int i = 0; i < 5; ++i)
    {
        NS_TEST_ASSERT_MSG_EQ(clientApps.Get(i)->GetStartTime().GetSeconds(), 2.0, "UDP echo client start time is incorrect.");
    }
}

// Test 4: Mobility model setup
void TestMobilityModel()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    // Set up mobility: arrange nodes in a circle
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Sink node at the center
    double radius = 20.0;
    for (int i = 1; i < 6; ++i)
    {
        double angle = i * (2 * M_PI / 5); // 5 peripheral nodes
        positionAlloc->Add(Vector(radius * cos(angle), radius * sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    // Check that nodes are placed in expected positions
    Vector pos = sensorNodes.Get(0)->GetObject<MobilityModel>()->GetPosition();
    NS_TEST_ASSERT_MSG_EQ(pos.x, 0.0, "Sink node position incorrect.");
    NS_TEST_ASSERT_MSG_EQ(pos.y, 0.0, "Sink node position incorrect.");

    for (int i = 1; i < 6; ++i)
    {
        pos = sensorNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        double angle = i * (2 * M_PI / 5);
        NS_TEST_ASSERT_MSG_EQ_TOL(pos.x, radius * cos(angle), 0.001, "Peripheral node position incorrect.");
        NS_TEST_ASSERT_MSG_EQ_TOL(pos.y, radius * sin(angle), 0.001, "Peripheral node position incorrect.");
    }
}

// Test 5: FlowMonitor statistics validation
void TestFlowMonitor()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    // Set up physical and 6LoWPAN layers
    LrWpanHelper lrWpan;
    NetDeviceContainer sensorDevices = lrWpan.Install(sensorNodes);
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(sensorDevices);

    // Install internet stack and IP addresses
    InternetStackHelper internet;
    internet.Install(sensorNodes);
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sensorInterfaces = ipv6.Assign(sixlowpanDevices);

    // Set up applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(sensorNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(sensorInterfaces.GetAddress(0, 1), 9);
    ApplicationContainer clientApps;
    for (int i = 1; i < 6; ++i)
    {
        clientApps.Add(echoClient.Install(sensorNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Install FlowMonitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Validate FlowMonitor statistics
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_NE(stats.size(), 0, "FlowMonitor did not capture any traffic.");

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        NS_TEST_ASSERT_MSG_GT(it->second.txPackets, 0, "No packets transmitted.");
        NS_TEST_ASSERT_MSG_GT(it->second.rxPackets, 0, "No packets received.");
        NS_TEST_ASSERT_MSG_LT(it->second.delaySum.GetSeconds(), 1.0, "End-to-End delay too high.");
    }

    Simulator::Destroy();
}

// Main function to run all tests
int main(int argc, char *argv[])
{
    TestNodeCreationAndDeviceInstallation();
    TestIpAddressAssignment();
    TestUdpEchoApplication();
    TestMobilityModel();
    TestFlowMonitor();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}

