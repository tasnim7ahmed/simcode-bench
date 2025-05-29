#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/bulk-send-application.h"
#include "ns3/tcp-westwood.h"
#include "ns3/enum.h"
#include "ns3/tcp-socket-base.h"
#include <fstream>
#include <iostream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

std::ofstream CWndTraceStream;
std::ofstream SsThreshTraceStream;
std::ofstream RttTraceStream;
std::ofstream RtoTraceStream;
std::ofstream InFlightTraceStream;

static void
CwndTracer(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string instanceNumber = "";
  std::string sinkNode = "";
  std::string appName = "";
  std::string ID = "";
  std::string tmpStr = "";
  context = context.substr(13, context.length());
  while ((pos = context.find(delimiter)) != std::string::npos)
  {
    token = context.substr(0, pos);
    if (token.find("ns3::TcpSocket") != std::string::npos)
    {
      appName = token;
    }
    context.erase(0, pos + delimiter.length());
  }

  ID = context;
  CWndTraceStream << Simulator::Now().GetSeconds() << " " << ID << " "
                  << oldCwnd << " " << newCwnd << std::endl;
}

static void
SsThreshTracer(std::string context, uint32_t oldValue, uint32_t newValue)
{
  std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string instanceNumber = "";
  std::string sinkNode = "";
  std::string appName = "";
  std::string ID = "";
  std::string tmpStr = "";
  context = context.substr(13, context.length());
  while ((pos = context.find(delimiter)) != std::string::npos)
  {
    token = context.substr(0, pos);
    if (token.find("ns3::TcpSocket") != std::string::npos)
    {
      appName = token;
    }
    context.erase(0, pos + delimiter.length());
  }

  ID = context;
  SsThreshTraceStream << Simulator::Now().GetSeconds() << " " << ID << " "
                      << oldValue << " " << newValue << std::endl;
}

static void
RttTracer(std::string context, Time oldValue, Time newValue)
{
  std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string instanceNumber = "";
  std::string sinkNode = "";
  std::string appName = "";
  std::string ID = "";
  std::string tmpStr = "";
  context = context.substr(13, context.length());
  while ((pos = context.find(delimiter)) != std::string::npos)
  {
    token = context.substr(0, pos);
    if (token.find("ns3::TcpSocket") != std::string::npos)
    {
      appName = token;
    }
    context.erase(0, pos + delimiter.length());
  }

  ID = context;
  RttTraceStream << Simulator::Now().GetSeconds() << " " << ID << " "
                 << oldValue.GetSeconds() << " " << newValue.GetSeconds()
                 << std::endl;
}

static void
RtoTracer(std::string context, Time oldValue, Time newValue)
{
  std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string instanceNumber = "";
  std::string sinkNode = "";
  std::string appName = "";
  std::string ID = "";
  std::string tmpStr = "";
  context = context.substr(13, context.length());
  while ((pos = context.find(delimiter)) != std::string::npos)
  {
    token = context.substr(0, pos);
    if (token.find("ns3::TcpSocket") != std::string::npos)
    {
      appName = token;
    }
    context.erase(0, pos + delimiter.length());
  }

  ID = context;
  RtoTraceStream << Simulator::Now().GetSeconds() << " " << ID << " "
                 << oldValue.GetSeconds() << " " << newValue.GetSeconds()
                 << std::endl;
}

static void
InFlightBytesTracer(std::string context, uint32_t oldValue, uint32_t newValue)
{
    std::string delimiter = "/";
    size_t pos = 0;
    std::string token;
    std::string instanceNumber = "";
    std::string sinkNode = "";
    std::string appName = "";
    std::string ID = "";
    std::string tmpStr = "";
    context = context.substr(13, context.length());
    while ((pos = context.find(delimiter)) != std::string::npos)
    {
        token = context.substr(0, pos);
        if (token.find("ns3::TcpSocket") != std::string::npos)
        {
            appName = token;
        }
        context.erase(0, pos + delimiter.length());
    }

    ID = context;
    InFlightTraceStream << Simulator::Now().GetSeconds() << " " << ID << " "
                      << oldValue << " " << newValue << std::endl;
}

int main(int argc, char* argv[])
{
  bool tracing = true;
  std::string bandwidth = "5Mbps";
  std::string delay = "2ms";
  double errorRate = 0.00001;
  std::string tcpVariant = "TcpWestwoodPlus";
  uint32_t maxBytes = 0;
  double duration = 10;
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue("bandwidth", "Link bandwidth", bandwidth);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("errorRate", "Packet error rate", errorRate);
  cmd.AddValue("tcpVariant", "TCP variant to use", tcpVariant);
  cmd.AddValue("maxBytes", "Maximum number of bytes to send", maxBytes);
  cmd.AddValue("duration", "Simulation duration", duration);
  cmd.AddValue("pcapTracing", "Enable pcap tracing", pcapTracing);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpVariant));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  pointToPoint.SetErrorModel(em);

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(
      InetSocketAddress(interfaces.GetAddress(1), sinkPort));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(0.0));
  sourceApps.Stop(Seconds(duration));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(duration));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  if (tracing)
  {
    std::string cWndFilename = "tcp-westwood-cwnd.dat";
    std::string ssThreshFilename = "tcp-westwood-ssthresh.dat";
    std::string rttFilename = "tcp-westwood-rtt.dat";
    std::string rtoFilename = "tcp-westwood-rto.dat";
    std::string inFlightFilename = "tcp-westwood-inflight.dat";

    CWndTraceStream.open(cWndFilename.c_str());
    SsThreshTraceStream.open(ssThreshFilename.c_str());
    RttTraceStream.open(rttFilename.c_str());
    RtoTraceStream.open(rtoFilename.c_str());
    InFlightTraceStream.open(inFlightFilename.c_str());

    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/0/SocketList/0/CongestionWindow",
        MakeCallback(&CwndTracer));
    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/0/SocketList/0/SlowStartThreshold",
        MakeCallback(&SsThreshTracer));
    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/0/SocketList/0/RTT",
        MakeCallback(&RttTracer));
    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/0/SocketList/0/RTO",
        MakeCallback(&RtoTracer));
      Config::ConnectWithoutContext(
          "/NodeList/0/ApplicationList/0/SocketList/0/BytesInFlight",
          MakeCallback(&InFlightBytesTracer));
  }

  if (pcapTracing)
  {
    pointToPoint.EnablePcapAll("tcp-westwood");
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(duration));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
       i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Flow Duration: " << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << "\n";
    std::cout << "  Throughput: "
              << i->second.rxBytes * 8.0 /
                     (i->second.timeLastRxPacket.GetSeconds() -
                      i->second.timeFirstTxPacket.GetSeconds()) /
                     1000000
              << " Mbps\n";
  }

  if (tracing)
  {
    CWndTraceStream.close();
    SsThreshTraceStream.close();
    RttTraceStream.close();
    RtoTraceStream.close();
    InFlightTraceStream.close();
  }

  Simulator::Destroy();
  return 0;
}