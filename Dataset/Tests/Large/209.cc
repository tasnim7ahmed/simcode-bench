#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTest");

class StarTopologyTestSuite : public TestSuite
{
public:
  StarTopologyTestSuite() : TestSuite("star-topology-test", UNIT)
  {
    AddTestCase(new StarTopologyTestCase(), TestCase::QUICK);
  }
};

class StarTopologyTestCase : public TestCase
{
public:
  StarTopologyTestCase() : TestCase("Test Star Topology Example") {}

  virtual void DoRun(void)
  {
    // Test 1: Node creation
    NodeContainer centralNode;
    centralNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(5);

    NS_TEST_ASSERT_MSG_EQ(centralNode.GetN(), 1, "Expected 1 central node");
    NS_TEST_ASSERT_MSG_EQ(clientNodes.GetN(), 5, "Expected 5 client nodes");

    // Test 2: Point-to-point link setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer serverDevices, clientDevices;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
      NodeContainer link = NodeContainer(centralNode.Get(0), clientNodes.Get(i));
      NetDeviceContainer devices = p2p.Install(link);
      serverDevices.Add(devices.Get(0));
      clientDevices.Add(devices.Get(1));
    }

    NS_TEST_ASSERT_MSG_EQ(serverDevices.GetN(), 5, "Expected 5 server devices");
    NS_TEST_ASSERT_MSG_EQ(clientDevices.GetN(), 5, "Expected 5 client devices");

    // Test 3: Internet stack installation
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(clientNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterfaces = address.Assign(serverDevices);
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);

    NS_TEST_ASSERT_MSG_EQ(serverInterfaces.GetN(), 5, "Expected 5 server interfaces");
    NS_TEST_ASSERT_MSG_EQ(clientInterfaces.GetN(), 5, "Expected 5 client interfaces");

    // Test 4: TCP server setup
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(serverInterfaces.GetAddress(0), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(centralNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Expected 1 TCP server application on the central node");

    // Test 5: TCP client setup on each client node
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
      clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i)));
      clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
      clientApps.Add(clientHelper.Install(clientNodes.Get(i)));
    }

    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 5, "Expected 5 TCP client applications on client nodes");

    // Test 6: Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Analyze Flow Monitor stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    bool throughputTestPassed = false;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.sourceAddress == clientInterfaces.GetAddress(0) && t.destinationAddress == serverInterfaces.GetAddress(0))
      {
        double throughput = i->second.rxBytes * 8.0 / (10.0 - 2.0) / 1000000; // 10s simulation, client starts at 2s
        NS_TEST_ASSERT_MSG_GT(throughput, 0.0, "Expected positive throughput from the flow");
        throughputTestPassed = true;
      }
    }

    NS_TEST_ASSERT_MSG_EQ(throughputTestPassed, true, "No valid throughput was measured");

    Simulator::Destroy();
  }
};

// Declare the test suite
static StarTopologyTestSuite starTopologyTestSuite;

int main(int argc, char *argv[])
{
  NS_LOG_UNCOND("Running Star Topology Test...");
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}

