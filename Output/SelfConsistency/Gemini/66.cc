#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-westwood.h"
#include "ns3/flow-monitor-module.h"

#include <fstream>
#include <iostream>
#include <string>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

// Global variables for tracing
std::map<Ptr<TcpSocket>, std::ofstream> CongWinStream;
std::map<Ptr<TcpSocket>, std::ofstream> SsThreshStream;
std::map<Ptr<TcpSocket>, std::ofstream> RttStream;
std::map<Ptr<TcpSocket>, std::ofstream> RtoStream;
std::map<Ptr<TcpSocket>, std::ofstream> InFlightBytesStream;

// Trace congestion window
static void
CwndTracer(std::ofstream *os, Time const &t, uint32_t const &cwnd)
{
  *os << t.GetSeconds() << " " << cwnd << std::endl;
}

// Trace slow start threshold
static void
SsThreshTracer(std::ofstream *os, Time const &t, uint32_t const &ssthresh)
{
  *os << t.GetSeconds() << " " << ssthresh << std::endl;
}

// Trace RTT
static void
RttTracer(std::ofstream *os, Time const &t, TimeValue const &rtt)
{
  *os << t.GetSeconds() << " " << rtt.Get().GetSeconds() << std::endl;
}

// Trace RTO
static void
RtoTracer(std::ofstream *os, Time const &t, Time const &rto)
{
  *os << t.GetSeconds() << " " << rto.GetSeconds() << std::endl;
}

// Trace InFlightBytes
static void
InFlightBytesTracer(std::ofstream *os, Time const &t, uint32_t const &inFlightBytes)
{
  *os << t.GetSeconds() << " " << inFlightBytes << std::endl;
}

int
main(int argc, char *argv[])
{
  // Set up tracing file names and parameters
  std::string bandwidth = "5Mbps";
  std::string delay = "2ms";
  double errorRate = 0.00001;
  std::string tcpVariant = "TcpWestwoodPlus"; // Default to TCP Westwood+
  bool tracing = true;
  double simTime = 10.0;

  // Allow command-line arguments to override defaults
  CommandLine cmd;
  cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue("delay", "Bottleneck delay", delay);
  cmd.AddValue("errorRate", "Packet error rate", errorRate);
  cmd.AddValue("tcpVariant", "TCP variant to use: TcpNewReno, TcpHybla, TcpHighSpeed, TcpVegas, TcpScalable, TcpWestwoodPlus", tcpVariant);
  cmd.AddValue("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue("simTime", "Simulation time (seconds)", simTime);

  cmd.Parse(argc, argv);

  // Configure TCP options
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpVariant));

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Create point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  // Set error model
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  pointToPoint.SetDeviceAttribute("ReceiveErrorModel", PointerValue(em));

  // Install net devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create applications (BulkSendApplication and PacketSink)
  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1448)); // Typical MTU size
  source.SetAttribute("MaxBytes", UintegerValue(0));    // Send unlimited data

  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(simTime));

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simTime));

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Tracing
  if (tracing)
  {
    // Access the TCP socket
    Ptr<Socket> socket = DynamicCast<BulkSendApplication>(sourceApps.Get(0))->GetSocket();
    Ptr<TcpSocket> tcpSocket = DynamicCast<TcpSocket>(socket);

    // Open trace files
    std::ofstream cwndStream;
    cwndStream.open((tcpVariant + "-cwnd.dat").c_str());
    CongWinStream[tcpSocket] = std::move(cwndStream);

    std::ofstream ssthreshStream;
    ssthreshStream.open((tcpVariant + "-ssthresh.dat").c_str());
    SsThreshStream[tcpSocket] = std::move(ssthreshStream);

    std::ofstream rttStream;
    rttStream.open((tcpVariant + "-rtt.dat").c_str());
    RttStream[tcpSocket] = std::move(rttStream);

    std::ofstream rtoStream;
    rtoStream.open((tcpVariant + "-rto.dat").c_str());
    RtoStream[tcpSocket] = std::move(rtoStream);

    std::ofstream inFlightBytesStream;
    inFlightBytesStream.open((tcpVariant + "-inflight.dat").c_str());
    InFlightBytesStream[tcpSocket] = std::move(inFlightBytesStream);

    // Connect trace sources
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/CongestionWindow", MakeBoundCallback(&CwndTracer, &CongWinStream[tcpSocket]));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/SlowStartThreshold", MakeBoundCallback(&SsThreshTracer, &SsThreshStream[tcpSocket]));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/RTT", MakeBoundCallback(&RttTracer, &RttStream[tcpSocket]));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/RTO", MakeBoundCallback(&RtoTracer, &RtoStream[tcpSocket]));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/InFlightBytes", MakeBoundCallback(&InFlightBytesTracer, &InFlightBytesStream[tcpSocket]));
  }

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Stop and print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
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

  monitor->SerializeToXmlFile("tcp-westwood-plus.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}