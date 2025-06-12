#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

class TraceHelper {
public:
  static void TraceCwnd(std::string cwndFile, Ptr<OutputStreamWrapper> stream) {
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&TraceHelper::CwndChange, stream));
  }

  static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }

  static void EnqueueDrop(std::string dropFile, Ptr<OutputStreamWrapper> stream) {
    Config::Connect("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime", MakeBoundCallback(&TraceHelper::SojournTime, stream));
    Config::Connect("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/Drop", MakeBoundCallback(&TraceHelper::QueueDrop, stream));
    Config::Connect("/NodeList/2/$ns3::TrafficControlLayer/RootQueueDiscList/0/Drop", MakeBoundCallback(&TraceHelper::QueueDrop, stream));
  }

  static void SojournTime(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal.GetMicroSeconds() << std::endl;
  }

  static void QueueDrop(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " DROP" << std::endl;
  }

  static void QueueSize(std::string file, Ptr<OutputStreamWrapper> stream) {
    Config::Connect("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/BytesInQueue", MakeBoundCallback(&TraceHelper::QueueSizeChange, stream));
    Config::Connect("/NodeList/2/$ns3::TrafficControlLayer/RootQueueDiscList/0/BytesInQueue", MakeBoundCallback(&TraceHelper::QueueSizeChange, stream));
  }

  static void QueueSizeChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(536));
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", DoubleValue(0));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  Config::SetDefault("ns3::TcpSocketBase::MinRttWarning", BooleanValue(false));

  NodeContainer nodes;
  nodes.Create(4);
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

  PointToPointHelper p2p0;
  p2p0.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2p0.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer dev0 = p2p0.Install(n0n1);

  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  p2p1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
  NetDeviceContainer dev1 = p2p1.Install(n1n2);

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2p2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer dev2 = p2p2.Install(n2n3);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(dev0);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(dev1);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign(dev2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
  bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = bulkSend.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceAds.Stop(Seconds(9.0));

  std::string resultsDir = "results/";
  std::string cwndFile = resultsDir + "cwndTraces/cwnd.dat";
  std::string queueSizeFile = resultsDir + "queue-size.dat";
  std::string queueDropFile = resultsDir + "queueTraces/drops.dat";
  std::string queueStatsFile = resultsDir + "queueStats.txt";
  std::string configFile = resultsDir + "config.txt";

  if (access(resultsDir.c_str(), F_OK) == -1) {
    system(("mkdir -p " + resultsDir).c_str());
    system(("mkdir -p " + resultsDir + "cwndTraces").c_str());
    system(("mkdir -p " + resultsDir + "pcap").c_str());
    system(("mkdir -p " + resultsDir + "queueTraces").c_str());
  }

  AsciiTraceHelper asciiTraceHelper;
  auto cwndStream = asciiTraceHelper.CreateFileStream(cwndFile);
  TraceHelper::TraceCwnd(cwndFile, cwndStream);

  auto queueDropStream = asciiTraceHelper.CreateFileStream(queueDropFile);
  TraceHelper::EnqueueDrop(queueDropFile, queueDropStream);

  auto queueSizeStream = asciiTraceHelper.CreateFileStream(queueSizeFile);
  TraceHelper::QueueSize(queueSizeFile, queueSizeStream);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  monitor->Start(Simulator::Now());

  p2p0.EnablePcapAll(resultsDir + "pcap/n0n1");
  p2p1.EnablePcapAll(resultsDir + "pcap/n1n2");
  p2p2.EnablePcapAll(resultsDir + "pcap/n2n3");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->Stop(Simulator::Now());
  monitor->SerializeToXmlFile(resultsDir + "flow.xml", false, false);

  std::ofstream configFile(configFile);
  configFile << "TcpSocket::SegmentSize: " << Config::GetDefault("ns3::TcpSocket::SegmentSize") << "\n";
  configFile << "TcpSocket::DelAckTimeout: " << Config::GetDefault("ns3::TcpSocket::DelAckTimeout") << "\n";
  configFile << "TcpL4Protocol::SocketType: " << Config::GetDefault("ns3::TcpL4Protocol::SocketType") << "\n";
  configFile.close();

  Simulator::Destroy();
  return 0;
}