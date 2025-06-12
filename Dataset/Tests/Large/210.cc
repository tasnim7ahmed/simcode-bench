#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologyTest");

class HybridTopologyTestSuite : public TestSuite
{
public:
  HybridTopologyTestSuite() : TestSuite("hybrid-topology-test", UNIT)
  {
    AddTestCase(new HybridTopologyTestCase(), TestCase::QUICK);
  }
};

class HybridTopologyTestCase : public TestCase
{
public:
  HybridTopologyTestCase() : TestCase("Test Hybrid Topology Example") {}

  virtual void DoRun(void)
  {
    TestStarTopology();
    TestMeshTopology();
    TestFlowMonitor();
  }

  void TestStarTopology()
  {
    NS_LOG_INFO("Test Star Topology - TCP setup");

    NodeContainer starNodes;
    starNodes.Create(6);

    NS_TEST_ASSERT_MSG_EQ(starNodes.GetN(), 6, "Expected 6 nodes in star topology");

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 1; i < starNodes.GetN(); ++i)
    {
      NodeContainer link = NodeContainer(starNodes.Get(0), starNodes.Get(i));
      NetDeviceContainer devices = p2p.Install(link);
      p2pDevices.Add(devices);
    }

    NS_TEST_ASSERT_MSG_EQ(p2pDevices.GetN(), 10, "Expected 10 devices in point-to-point setup");

    InternetStackHelper internet;
    internet.Install(starNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer starInterfaces = address.Assign(p2pDevices);

    uint16_t tcpPort = 9;
    Address tcpServerAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));

    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpServerAddress);
    ApplicationContainer tcpServerApp = tcpSinkHelper.Install(starNodes.Get(0));
    tcpServerApp.Start(Seconds(1.0));
    tcpServerApp.Stop(Seconds(20.0));

    NS_TEST_ASSERT_MSG_EQ(tcpServerApp.GetN(), 1, "Expected 1 TCP server application");

    OnOffHelper tcpClientHelper("ns3::TcpSocketFactory", InetSocketAddress(starNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), tcpPort));
    tcpClientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
    tcpClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer tcpClientApps;
    for (uint32_t i = 1; i < starNodes.GetN(); ++i)
    {
      tcpClientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i)));
      tcpClientHelper.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
      tcpClientApps.Add(tcpClientHelper.Install(starNodes.Get(i)));
    }

    NS_TEST_ASSERT_MSG_EQ(tcpClientApps.GetN(), 5, "Expected 5 TCP client applications");
  }

  void TestMeshTopology()
  {
    NS_LOG_INFO("Test Mesh Topology - UDP setup");

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NS_TEST_ASSERT_MSG_EQ(meshNodes.GetN(), 4, "Expected 4 nodes in mesh topology");

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

    uint16_t udpPort = 8080;
    UdpEchoServerHelper udpServer(udpPort);
    ApplicationContainer udpServerApp = udpServer.Install(meshNodes.Get(0));
    udpServerApp.Start(Seconds(1.0));
    udpServerApp.Stop(Seconds(20.0));

    NS_TEST_ASSERT_MSG_EQ(udpServerApp.GetN(), 1, "Expected 1 UDP server application");

    UdpEchoClientHelper udpClient(meshInterfaces.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer udpClientApps;
    for (uint32_t i = 1; i < meshNodes.GetN(); ++i)
    {
      udpClient.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i)));
      udpClient.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
      udpClientApps.Add(udpClient.Install(meshNodes.Get(i)));
    }

    NS_TEST_ASSERT_MSG_EQ(udpClientApps.GetN(), 3, "Expected 3 UDP client applications");
  }

  void TestFlowMonitor()
  {
    NS_LOG_INFO("Test Flow Monitor");

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    bool foundTcpFlow = false;
    bool foundUdpFlow = false;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.destinationPort == 9)
      {
        foundTcpFlow = true;
        NS_TEST_ASSERT_MSG_GT(i->second.rxBytes, 0, "Expected TCP flow to receive data");
      }
      if (t.destinationPort == 8080)
      {
        foundUdpFlow = true;
        NS_TEST_ASSERT_MSG_GT(i->second.rxBytes, 0, "Expected UDP flow to receive data");
      }
    }

    NS_TEST_ASSERT_MSG_EQ(foundTcpFlow, true, "No TCP flow was found");
    NS_TEST_ASSERT_MSG_EQ(foundUdpFlow, true, "No UDP flow was found");

    Simulator::Destroy();
  }
};

// Declare the test suite
static HybridTopologyTestSuite hybridTopologyTestSuite;

int main(int argc, char *argv[])
{
  NS_LOG_UNCOND("Running Hybrid Topology Test...");
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}

