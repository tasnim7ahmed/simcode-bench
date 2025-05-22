#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

class P2PTestSuite : public TestCase
{
public:
    P2PTestSuite() : TestCase("Test P2P Network Example") {}
    virtual ~P2PTestSuite() {}

    void DoRun() override
    {
        // Call the helper functions for testing different parts of the network setup
        TestNodeCreation();
        TestPointToPointSetup();
        TestInternetStackInstallation();
        TestRoutingSetup();
        TestTCPClientServerApplications();
        TestFlowMonitor();
        TestPcapTracing();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(3); // Node 0 -> Client, Node 1 -> Router, Node 2 -> Server
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed. Expected 3 nodes.");
    }

    // Test the point-to-point link configuration
    void TestPointToPointSetup()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

        // Verify that two point-to-point devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices01.GetN(), 1, "Failed to install point-to-point devices between nodes 0 and 1.");
        NS_TEST_ASSERT_MSG_EQ(devices12.GetN(), 1, "Failed to install point-to-point devices between nodes 1 and 2.");
    }

    // Test the installation of Internet stack on nodes
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed on node " << i);
        }
    }

    // Test the IP address assignment and routing setup
    void TestRoutingSetup()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // Verify routing tables are populated for nodes
        Ptr<Ipv4RoutingProtocol> routingProtocol = nodes.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "Routing protocol not installed on node 0.");
    }

    // Test the TCP client and server application setup
    void TestTCPClientServerApplications()
    {
        NodeContainer nodes;
        nodes.Create(3);

        uint16_t port = 9;
        Address serverAddress(InetSocketAddress("10.1.2.2", port));

        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp;
        clientHelper.SetAttribute("Remote", AddressValue(serverAddress));
        clientApp = clientHelper.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client and server applications are installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Server application setup failed.");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Client application setup failed.");
    }

    // Test the flow monitor to collect throughput and packet loss statistics
    void TestFlowMonitor()
    {
        NodeContainer nodes;
        nodes.Create(3);

        FlowMonitorHelper flowMonitor;
        Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

        Simulator::Run();

        monitor->CheckForLostPackets();
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        NS_TEST_ASSERT_MSG_NOT_EMPTY(stats, "Flow monitor statistics are empty.");

        // Check that flow statistics include transmitted and received packets
        for (auto const &stat : stats)
        {
            NS_TEST_ASSERT_MSG_GREATER(stat.second.txPackets, 0, "No transmitted packets in flow " << stat.first);
            NS_TEST_ASSERT_MSG_GREATER(stat.second.rxPackets, 0, "No received packets in flow " << stat.first);
        }
    }

    // Test PCAP tracing functionality
    void TestPcapTracing()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

        // Enable PCAP tracing
        pointToPoint.EnablePcapAll("p2p-network");

        // Check if pcap files are generated (you will need to manually check after running the test)
        NS_TEST_ASSERT_MSG_TRUE(FileExists("p2p-network-0-0.pcap"), "PCAP trace file not generated.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("P2PTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new P2PTestSuite());
    return TestSuiteInstance->Run();
}
