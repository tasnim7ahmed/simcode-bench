#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNodeTcpSimulation");

class CwndTracer {
public:
  static void TraceCwnd(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }
};

int main(int argc, char *argv[]) {
  // Configuration
  std::string resultDir = "results/";
  std::string cwndTraceFile = resultDir + "cwndTraces/cwnd_trace.txt";
  std::string queueSizeFile = resultDir + "queue-size.dat";
  std::string pcapDir = resultDir + "pcap/";
  std::string queueDropTraceFile = resultDir + "queueTraces/drop_traces.txt";
  std::string queueStatsFile = resultDir + "queueStats.txt";
  std::string configOutputFile = resultDir + "config.txt";

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketBase::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(512));

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Links: n0-n1 (10 Mbps, 1ms), n1-n2 (1 Mbps, 10ms), n2-n3 (10 Mbps, 1ms)
  PointToPointHelper p2p_n0n1;
  p2p_n0n1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2p_n0n1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  p2p_n0n1.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

  PointToPointHelper p2p_n1n2;
  p2p_n1n2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  p2p_n1n2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
  p2p_n1n2.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

  PointToPointHelper p2p_n2n3;
  p2p_n2n3.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2p_n2n3.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  p2p_n2n3.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Create devices and channels
  NetDeviceContainer d_n0n1 = p2p_n0n1.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d_n1n2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d_n2n3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n0n1 = address.Assign(d_n0n1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n1n2 = address.Assign(d_n1n2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n2n3 = address.Assign(d_n2n3);

  // Set up global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Applications: TCP flow from n0 to n3
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(i_n2n3.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
  bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
  ApplicationContainer sourceApp = bulkSend.Install(nodes.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(10.0));

  // Output directories
  system(("mkdir -p " + resultDir).c_str());
  system(("mkdir -p " + resultDir + "cwndTraces").c_str());
  system(("mkdir -p " + resultDir + "pcap").c_str());
  system(("mkdir -p " + resultDir + "queueTraces").c_str());

  // Tracing
  AsciiTraceHelper asciiTraceHelper;
  p2p_n0n1.EnableAsciiAll(asciiTraceHelper.CreateFileStream(resultDir + "n0n1.tr"));
  p2p_n1n2.EnableAsciiAll(asciiTraceHelper.CreateFileStream(resultDir + "n1n2.tr"));
  p2p_n2n3.EnableAsciiAll(asciiTraceHelper.CreateFileStream(resultDir + "n2n3.tr"));

  p2p_n0n1.EnablePcapAll(pcapDir + "n0n1");
  p2p_n1n2.EnablePcapAll(pcapDir + "n1n2");
  p2p_n2n3.EnablePcapAll(pcapDir + "n2n3");

  // CWND tracing
  Ptr<OutputStreamWrapper> cwndStream = CreateFileStreamWrapper(cwndTraceFile, std::ios::out);
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndTracer::TraceCwnd, cwndStream));

  // Queue size monitoring for n1n2 link
  Ptr<PointToPointNetDevice> devTx_n1 = DynamicCast<PointToPointNetDevice>(d_n1n2.Get(0));
  if (devTx_n1) {
    Ptr<Queue> queue = devTx_n1->GetQueue();
    if (queue) {
      queue->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&CwndTracer::TraceCwnd, CreateFileStreamWrapper(queueSizeFile, std::ios::out)));
      queue->TraceConnectWithoutContext("Drop", MakeBoundCallback(&CwndTracer::TraceCwnd, CreateFileStreamWrapper(queueDropTraceFile, std::ios::out)));
    }
  }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Queue stats
  QueueDiscContainer qdiscs;
  qdiscs = p2p_n0n1.GetQueueDiscs(d_n0n1);
  qdiscs.Add(p2p_n1n2.GetQueueDiscs(d_n1n2));
  qdiscs.Add(p2p_n2n3.GetQueueDiscs(d_n2n3));
  qdiscs.EnableAsciiAll(CreateFileStreamWrapper(queueStatsFile, std::ios::out));

  // Write configuration file
  std::ofstream configFile(configOutputFile);
  configFile << "Topology: Four-node series network\n";
  configFile << "Link n0-n1: 10 Mbps, 1 ms delay\n";
  configFile << "Link n1-n2: 1 Mbps, 10 ms delay\n";
  configFile << "Link n2-n3: 10 Mbps, 1 ms delay\n";
  configFile << "Application: TCP BulkSend from n0 to n3 on port " << sinkPort << "\n";
  configFile << "Simulation duration: 10 seconds\n";
  configFile.close();

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    NS_LOG_INFO("Flow ID: " << iter->first << " Info: " << iter->second);
  }

  Simulator::Destroy();
  return 0;
}