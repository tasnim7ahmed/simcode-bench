#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class ManetTestSuite : public TestCase
{
public:
    ManetTestSuite() : TestCase("Test AODV MANET Example") {}
    virtual ~ManetTestSuite() {}

    void DoRun() override
    {
        // Call the helper functions for testing different parts of the network setup
        TestNodeCreation();
        TestWifiSetup();
        TestRoutingProtocol();
        TestMobilitySetup();
        TestApplicationSetup();
        TestFlowMonitor();
        TestPcapTracing();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(4);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node creation failed. Expected 4 nodes.");
    }

    // Test Wi-Fi setup and configuration
    void TestWifiSetup()
    {
        NodeContainer nodes;
        nodes.Create(4);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate2Mbps"));

        wifiMac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

        NS_TEST_ASSERT_MSG_EQ(wifiDevices.GetN(), 4, "Wi-Fi setup failed. Expected 4 devices.");
    }

    // Test the AODV routing protocol installation
    void TestRoutingProtocol()
    {
        NodeContainer nodes;
        nodes.Create(4);

        AodvHelper aodv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(aodv);
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes.Get(0)->GetObject<NetDeviceContainer>());

        // Check if the AODV routing protocol is successfully installed
        Ptr<Ipv4RoutingProtocol> routing = nodes.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routing, "AODV routing protocol installation failed.");
    }

    // Test mobility configuration
    void TestMobilitySetup()
    {
        NodeContainer nodes;
        nodes.Create(4);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(nodes);

        // Check if mobility has been installed correctly on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model installation failed on node " << i);
        }
    }

    // Test the application setup (UDP client and server)
    void TestApplicationSetup()
    {
        NodeContainer nodes;
        nodes.Create(4);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(0, 0).GetLocal(), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Server application setup failed.");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Client application setup failed.");
    }

    // Test flow monitoring and packet statistics
    void TestFlowMonitor()
    {
        NodeContainer nodes;
        nodes.Create(4);

        FlowMonitorHelper flowmonHelper;
        Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

        Simulator::Run();

        monitor->CheckForLostPackets();
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        NS_TEST_ASSERT_MSG_NOT_EMPTY(stats, "Flow monitor statistics are empty.");

        // Check if any flow has non-zero transmitted and received packets
        for (auto const &stat : stats)
        {
            NS_TEST_ASSERT_MSG_GREATER(stat.second.txPackets, 0, "No transmitted packets in flow " << stat.first);
            NS_TEST_ASSERT_MSG_GREATER(stat.second.rxPackets, 0, "No received packets in flow " << stat.first);
        }
    }

    // Test pcap tracing functionality
    void TestPcapTracing()
    {
        NodeContainer nodes;
        nodes.Create(4);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate2Mbps"));

        wifiMac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

        // Enable pcap tracing
        wifiPhy.EnablePcap("manet", wifiDevices);

        // Check if pcap files are generated (you would need to manually check after running the test)
        NS_TEST_ASSERT_MSG_TRUE(FileExists("manet-0-0.pcap"), "PCAP trace file not generated.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("ManetSimulationTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new ManetTestSuite());
    return TestSuiteInstance->Run();
}
