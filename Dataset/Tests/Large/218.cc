#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

class VanetDsdvTestSuite : public TestCase
{
public:
    VanetDsdvTestSuite() : TestCase("Test DSDV VANET Example") {}
    virtual ~VanetDsdvTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestMobilityModel();
        TestDsdvRouting();
        TestUdpApplications();
        TestFlowMonitor();
        TestPcapAndAnimationTracing();
    }

private:
    // Test the creation of nodes (vehicles)
    void TestNodeCreation()
    {
        NodeContainer vehicles;
        vehicles.Create(10);
        NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), 10, "Node creation failed. Expected 10 vehicles.");
    }

    // Test the WiFi physical and MAC layer setup
    void TestWifiSetup()
    {
        NodeContainer vehicles;
        vehicles.Create(10);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 10, "WiFi setup failed. Expected 10 devices.");
    }

    // Test the mobility model setup for vehicles
    void TestMobilityModel()
    {
        NodeContainer vehicles;
        vehicles.Create(10);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(50.0),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(10),
                                      "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(vehicles);

        // Check velocity of first vehicle
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mob->GetVelocity(), Vector(20.0, 0.0, 0.0), "Incorrect velocity model for vehicle 0.");
    }

    // Test DSDV routing setup
    void TestDsdvRouting()
    {
        NodeContainer vehicles;
        vehicles.Create(10);

        DsdvHelper dsdv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(dsdv);
        internet.Install(vehicles);

        // Check if DSDV is installed correctly
        for (uint32_t i = 0; i < vehicles.GetN(); ++i)
        {
            Ptr<Ipv4RoutingProtocol> routingProtocol = vehicles.Get(i)->GetObject<Ipv4RoutingProtocol>();
            NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "DSDV routing protocol not installed on node " << i);
        }
    }

    // Test UDP client-server applications setup
    void TestUdpApplications()
    {
        NodeContainer vehicles;
        vehicles.Create(10);

        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(vehicles.Get(9));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        UdpClientHelper udpClient(vehicles.Get(9)->GetObject<Ipv4>()->GetAddress(1), 9);
        udpClient.SetAttribute("MaxPackets", UintegerValue(320));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(vehicles.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        // Verify that client and server applications are installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application setup failed.");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application setup failed.");
    }

    // Test flow monitor functionality to track performance metrics
    void TestFlowMonitor()
    {
        NodeContainer vehicles;
        vehicles.Create(10);

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
        NodeContainer vehicles;
        vehicles.Create(10);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

        // Enable PCAP tracing
        wifiPhy.EnablePcap("vanet-dsdv", devices);

        // Create animation interface
        AnimationInterface anim("vanet-dsdv.xml");

        // Verify that PCAP file is generated
        NS_TEST_ASSERT_MSG_TRUE(FileExists("vanet-dsdv-0-0.pcap"), "PCAP trace file not generated.");
        NS_TEST_ASSERT_MSG_TRUE(FileExists("vanet-dsdv.xml"), "Animation file not generated.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("VanetDsdvTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new VanetDsdvTestSuite());
    return TestSuiteInstance->Run();
}
