#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

class AodvAdhocTestSuite : public TestCase
{
public:
    AodvAdhocTestSuite() : TestCase("Test AODV Ad-Hoc Network Example") {}
    virtual ~AodvAdhocTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestAodvRouting();
        TestMobilityModel();
        TestUdpApplications();
        TestFlowMonitor();
        TestPcapAndAnimationTracing();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(5);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 5, "Node creation failed. Expected 5 nodes.");
    }

    // Test the WiFi physical and MAC layer setup
    void TestWifiSetup()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel;
        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 5, "WiFi setup failed. Expected 5 devices.");
    }

    // Test AODV routing setup
    void TestAodvRouting()
    {
        NodeContainer nodes;
        nodes.Create(5);

        AodvHelper aodv;
        Ipv4ListRoutingHelper list;
        list.Add(aodv, 100);
        InternetStackHelper internet;
        internet.SetRoutingHelper(list);
        internet.Install(nodes);

        // Check if AODV routing protocol is installed
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4RoutingProtocol> routingProtocol = nodes.Get(i)->GetObject<Ipv4RoutingProtocol>();
            NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "AODV routing protocol not installed on node " << i);
        }
    }

    // Test mobility model for the nodes
    void TestMobilityModel()
    {
        NodeContainer nodes;
        nodes.Create(5);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                  "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"));
        mobility.Install(nodes);

        // Verify that mobility model is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on node " << i);
        }
    }

    // Test UDP client-server applications setup
    void TestUdpApplications()
    {
        NodeContainer nodes;
        nodes.Create(5);

        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(4));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(nodes.Get(4)->GetObject<Ipv4>()->GetAddress(1), 9);
        udpClient.SetAttribute("MaxPackets", UintegerValue(320));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that client and server applications are installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application setup failed.");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application setup failed.");
    }

    // Test flow monitor functionality to track performance metrics
    void TestFlowMonitor()
    {
        NodeContainer nodes;
        nodes.Create(5);

        FlowMonitorHelper flowMonitor;
        Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

        Simulator::Run();

        monitor->CheckForLostPackets();
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        NS_TEST_ASSERT_MSG_NOT_EMPTY(stats, "Flow monitor statistics are empty.");

        // Check if flow statistics include transmitted and received packets
        for (auto const &stat : stats)
        {
            NS_TEST_ASSERT_MSG_GREATER(stat.second.txPackets, 0, "No transmitted packets in flow " << stat.first);
            NS_TEST_ASSERT_MSG_GREATER(stat.second.rxPackets, 0, "No received packets in flow " << stat.first);
        }
    }

    // Test PCAP and Animation tracing functionality
    void TestPcapAndAnimationTracing()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel;
        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        // Enable PCAP tracing
        wifiPhy.EnablePcap("adhoc-aodv", devices);

        // Create animation interface
        AnimationInterface anim("adhoc-aodv.xml");

        // Verify that PCAP file is generated
        NS_TEST_ASSERT_MSG_TRUE(FileExists("adhoc-aodv-0-0.pcap"), "PCAP trace file not generated.");
        NS_TEST_ASSERT_MSG_TRUE(FileExists("adhoc-aodv.xml"), "Animation file not generated.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("AodvAdhocTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new AodvAdhocTestSuite());
    return TestSuiteInstance->Run();
}
