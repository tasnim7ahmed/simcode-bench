#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

class TcpWestwoodPlusTracer {
public:
  static void TraceCongestionWindow(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
  }

  static void TraceSlowStartThreshold(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
  }

  static void TraceRtt(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
  }

  static void TraceRto(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
  }

  static void TraceInFlight(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
  }
};

int main(int argc, char *argv[]) {
  std::string tcpVariant = "TcpWestwood";
  std::string bandwidth = "5Mbps";
  std::string delay = "20ms";
  double errorRate = 0.0;
  uint32_t mtuBytes = 1400;
  uint32_t maxBytes = 1000000;
  bool enableFlowMonitor = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("tcpVariant", "Transport protocol to use: TcpWestwood or TcpWestwoodPlus", tcpVariant);
  cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue("delay", "Bottleneck delay", delay);
  cmd.AddValue("errorRate", "Packet error rate (e.g., 0.001)", errorRate);
  cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse(argc, argv);

  if (tcpVariant == "TcpWestwoodPlus") {
    Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(ns3::TcpWestwood::WESTWOODPLUS));
  }

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(mtuBytes));
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", DoubleValue(0.0));
  Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.1)));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpVariant)));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  if (errorRate > 0) {
    pointToPoint.SetDeviceAttribute("ReceiveErrorModel",
                                    PointerValue(CreateObject<RateErrorModel>()));
    Config::SetDefault("ns3::RateErrorModel::ErrorRate", DoubleValue(errorRate));
  }

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper sender("ns3::TcpSocketFactory", sinkAddress);
  sender.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer senderApps = sender.Install(nodes.Get(0));
  senderApps.Start(Seconds(1.0));
  senderApps.Stop(Seconds(10.0));

  AsciiTraceHelper asciiTraceHelper;
  pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-westwood-plus.tr"));
  pointToPoint.EnablePcapAll("tcp-westwood-plus");

  std::ofstream cWndStream;
  std::ofstream ssThreshStream;
  std::ofstream rttStream;
  std::ofstream rtoStream;
  std::ofstream inFlightStream;

  cWndStream.open("cwnd.dat");
  ssThreshStream.open("ssthresh.dat");
  rttStream.open("rtt.dat");
  rtoStream.open("rto.dat");
  inFlightStream.open("inflight.dat");

  Ptr<OutputStreamWrapper> cWndWrapper = asciiTraceHelper.CreateFileStream("cwnd.dat");
  Ptr<OutputStreamWrapper> ssThreshWrapper = asciiTraceHelper.CreateFileStream("ssthresh.dat");
  Ptr<OutputStreamWrapper> rttWrapper = asciiTraceHelper.CreateFileStream("rtt.dat");
  Ptr<OutputStreamWrapper> rtoWrapper = asciiTraceHelper.CreateFileStream("rto.dat");
  Ptr<OutputStreamWrapper> inFlightWrapper = asciiTraceHelper.CreateFileStream("inflight.dat");

  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                MakeBoundCallback(&TcpWestwoodPlusTracer::TraceCongestionWindow, cWndWrapper));
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                                MakeBoundCallback(&TcpWestwoodPlusTracer::TraceSlowStartThreshold, ssThreshWrapper));
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RttSample",
                                MakeBoundCallback(&TcpWestwoodPlusTracer::TraceRtt, rttWrapper));
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTO",
                                MakeBoundCallback(&TcpWestwoodPlusTracer::TraceRto, rtoWrapper));
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight",
                                MakeBoundCallback(&TcpWestwoodPlusTracer::TraceInFlight, inFlightWrapper));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
  if (enableFlowMonitor) {
    monitor = flowmon.InstallAll();
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  if (enableFlowMonitor) {
    flowmon.SerializeToXmlFile("tcp-westwood-plus.flowmon", false, false);
  }

  Simulator::Destroy();

  cWndStream.close();
  ssThreshStream.close();
  rttStream.close();
  rtoStream.close();
  inFlightStream.close();

  return 0;
}