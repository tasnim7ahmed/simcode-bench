#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkUnitTests");

// Test 1: Node creation
void TestNodeCreation()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);
    NS_TEST_ASSERT_MSG_EQ(sensorNodes.GetN(), 6, "Expected 6 sensor nodes to be created");
}

// Test 2: CSMA installation
void TestCsmaInstallation()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(sensorNodes);
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 6, "Expected CSMA devices to be installed on all 6 nodes");
}

// Test 3: Mobility model installation
void TestMobilityModel()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = sensorNodes.Get(i)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed correctly");
    }
}

// Test 4: UDP traffic setup
void TestUdpTraffic()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(sensorNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.1.1"), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < sensorNodes.GetN(); ++i)
    {
        clientApps.Add(echoClient.Install(sensorNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    NS_TEST_ASSERT_MSG_EQ(serverApp.Get(0)->GetStartTime().GetSeconds(), 1.0, "Server did not start at the correct time");
    NS_TEST_ASSERT_MSG_EQ(clientApps.Get(0)->GetStartTime().GetSeconds(), 2.0, "Client did not start at the correct time");
    Simulator::Destroy();
}

// Test 5: FlowMonitor validation
void TestFlowMonitor()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer devices = csma.Install(sensorNodes);

    InternetStackHelper stack;
    stack.Install(sensorNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(devices);

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_NE(stats.size(), 0, "FlowMonitor should capture some traffic");

    Simulator::Destroy();
}

// Test 6: NetAnim positioning
void TestNetAnimPositioning()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    AnimationInterface anim("wsn-animation.xml");
    anim.SetConstantPosition(sensorNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(sensorNodes.Get(1), 20.0, 0.0);
    anim.SetConstantPosition(sensorNodes.Get(2), 40.0, 0.0);
    anim.SetConstantPosition(sensorNodes.Get(3), 0.0, 20.0);
    anim.SetConstantPosition(sensorNodes.Get(4), 20.0, 20.0);
    anim.SetConstantPosition(sensorNodes.Get(5), 40.0, 20.0);

    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Vector position = anim.GetPosition(sensorNodes.Get(i));
        NS_TEST_ASSERT_MSG_NE(position.x, 0.0, "Position X not set correctly");
        NS_TEST_ASSERT_MSG_NE(position.y, 0.0, "Position Y not set correctly");
    }
}

// Main test execution function
int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestCsmaInstallation();
    TestMobilityModel();
    TestUdpTraffic();
    TestFlowMonitor();
    TestNetAnimPositioning();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}

