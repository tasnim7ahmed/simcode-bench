#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

std::map<Ptr<TcpSocket>, std::ofstream> CongestionWindowStreams;
std::map<Ptr<TcpSocket>, std::ofstream> SlowStartThresholdStreams;
std::map<Ptr<TcpSocket>, std::ofstream> RttStreams;
std::map<Ptr<TcpSocket>, std::ofstream> RtoStreams;
std::map<Ptr<TcpSocket>, std::ofstream> InFlightBytesStreams;

static void
CwndTracer(std::ofstream *stream, Time const &ts, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream << ts.GetSeconds() << " " << oldCwnd << " " << newCwnd << std::endl;
}

static void
SsThreshTracer(std::ofstream *stream, Time const &ts, uint32_t oldSsThresh, uint32_t newSsThresh)
{
  *stream << ts.GetSeconds() << " " << oldSsThresh << " " << newSsThresh << std::endl;
}

static void
RttTracer(std::ofstream *stream, Time const &ts, Time const &oldRtt, Time const &newRtt)
{
  *stream << ts.GetSeconds() << " " << oldRtt.GetSeconds() << " " << newRtt.GetSeconds() << std::endl;
}

static void
RtoTracer(std::ofstream *stream, Time const &ts, Time const &oldRto, Time const &newRto)
{
  *stream << ts.GetSeconds() << " " << oldRto.GetSeconds() << " " << newRto.GetSeconds() << std::endl;
}

static void
InFlightBytesTracer(std::ofstream *stream, Time const &ts, uint32_t oldInFlightBytes, uint32_t newInFlightBytes)
{
  *stream << ts.GetSeconds() << " " << oldInFlightBytes << " " << newInFlightBytes << std::endl;
}


int
main(int argc, char *argv[])
{
  CommandLine cmd;
  std::string bandwidth = "5Mbps";
  std::string delay = "2ms";
  double errorRate = 0.0;
  std::string tcpVariant = "TcpWestwoodPlus";
  int numFlows = 1;
  std::string prefix = "tcp-westwood-plus";

  cmd.AddValue("bandwidth", "Link bandwidth", bandwidth);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("errorRate", "Packet error rate", errorRate);
  cmd.AddValue("tcpVariant", "TCP variant to use: TcpWestwoodPlus, TcpLinuxReno", tcpVariant);
  cmd.AddValue("numFlows", "Number of flows", numFlows);
  cmd.AddValue("prefix", "Prefix for output files", prefix);

  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpVariant));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));
  pointToPoint.SetQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p"));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  if (errorRate > 0.0)
    {
      Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
      em->SetAttribute("ErrorRate", DoubleValue(errorRate));
      devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;

  for (int i = 0; i < numFlows; ++i)
    {
      BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port + i));
      source.SetAttribute("SendSize", UintegerValue(1448));
      source.SetAttribute("MaxBytes", UintegerValue(0));

      ApplicationContainer sourceApps = source.Install(nodes.Get(0));
      sourceApps.Start(Seconds(1.0));
      sourceApps.Stop(Seconds(10.0));

      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
      ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
      sinkApps.Start(Seconds(0.0));
      sinkApps.Stop(Seconds(10.0));

      // Tracing
      Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
      socket->Connect(InetSocketAddress(interfaces.GetAddress(1), port + i));

      CongestionWindowStreams[socket] = std::ofstream(prefix + "-cwnd-" + std::to_string(i) + ".dat");
      SlowStartThresholdStreams[socket] = std::ofstream(prefix + "-ssthresh-" + std::to_string(i) + ".dat");
      RttStreams[socket] = std::ofstream(prefix + "-rtt-" + std::to_string(i) + ".dat");
      RtoStreams[socket] = std::ofstream(prefix + "-rto-" + std::to_string(i) + ".dat");
      InFlightBytesStreams[socket] = std::ofstream(prefix + "-inflight-" + std::to_string(i) + ".dat");

      Config::ConnectWithoutContext(
          "/ChannelList/0/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + std::to_string(i) + "/CongestionWindow",
          MakeBoundCallback(&CwndTracer, &CongestionWindowStreams[socket]));
      Config::ConnectWithoutContext(
          "/ChannelList/0/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + std::to_string(i) + "/SlowStartThreshold",
          MakeBoundCallback(&SsThreshTracer, &SlowStartThresholdStreams[socket]));
      Config::ConnectWithoutContext(
          "/ChannelList/0/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + std::to_string(i) + "/RTT",
          MakeBoundCallback(&RttTracer, &RttStreams[socket]));
      Config::ConnectWithoutContext(
          "/ChannelList/0/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + std::to_string(i) + "/RTO",
          MakeBoundCallback(&RtoTracer, &RtoStreams[socket]));
      Config::ConnectWithoutContext(
          "/ChannelList/0/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + std::to_string(i) + "/BytesInFlight",
          MakeBoundCallback(&InFlightBytesTracer, &InFlightBytesStreams[socket]));

    }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

  monitor->SerializeToXmlFile(prefix + "-flowmon.xml", false, true);

  Simulator::Destroy();

  for (auto &stream : CongestionWindowStreams)
    {
      stream.second.close();
    }
  for (auto &stream : SlowStartThresholdStreams)
    {
      stream.second.close();
    }
  for (auto &stream : RttStreams)
    {
      stream.second.close();
    }
  for (auto &stream : RtoStreams)
    {
      stream.second.close();
    }
   for (auto &stream : InFlightBytesStreams)
    {
      stream.second.close();
    }

  return 0;
}