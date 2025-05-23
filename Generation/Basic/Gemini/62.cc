#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-pacing-strategy.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

Ptr<OutputStreamWrapper> g_cwndStream;
Ptr<OutputStreamWrapper> g_pacingRateStream;

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  *g_cwndStream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

static void
PacingRateChange (const DataRate& oldPacingRate, const DataRate& newPacingRate)
{
  *g_pacingRateStream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newPacingRate.GetBitRate () << std::endl;
}

int
main (int argc, char* argv[])
{
  uint32_t simulationTime = 10; // seconds
  uint32_t initialCwnd = 10;    // segments
  bool enablePacing = true;
  std::string outputFileName = "tcp-pacing-scenario";
  std::string tcpVariant = "ns3::TcpNewReno";

  CommandLine cmd(__FILE__);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("initialCwnd", "Initial congestion window (segments)", initialCwnd);
  cmd.AddValue ("enablePacing", "Enable TCP pacing (true/false)", enablePacing);
  cmd.AddValue ("outputFileName", "Prefix for output trace files (e.g., .dat, .xml, .tr)", outputFileName);
  cmd.AddValue ("tcpVariant", "TCP variant to use (e.g., ns3::TcpNewReno)", tcpVariant);
  cmd.Parse (argc, argv);

  // Configure TCP parameters
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeValue (TypeId::LookupByName (tcpVariant)));
  Config::SetDefault ("ns3::TcpSocketBase::MinRTO", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::TcpSocketBase::SndBufSize", UintegerValue (131072)); // 128 KB
  Config::SetDefault ("ns3::TcpSocketBase::RcvBufSize", UintegerValue (131072)); // 128 KB
  Config::SetDefault ("ns3::TcpSocketBase::InitialCwnd", UintegerValue (initialCwnd));

  if (enablePacing)
    {
      Config::SetDefault ("ns3::TcpSocketBase::PacingStrategy", EnumValue (TcpPacingStrategy::TCP_PACING_APPROX_FAIR));
    }
  else
    {
      Config::SetDefault ("ns3::TcpSocketBase::PacingStrategy", EnumValue (TcpPacingStrategy::TCP_PACING_NONE));
    }

  // Create nodes
  NodeContainer nodes;
  nodes.Create (6); // n0, n1, n2, n3, n4, n5

  // Install Internet Stack on all nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // Configure Point-to-Point Helper
  PointToPointHelper p2pHelper;

  // Link n0-n1: 10Mbps, 10ms
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer d0d1 = p2pHelper.Install (nodes.Get (0), nodes.Get (1));

  // Link n1-n2: 10Mbps, 10ms
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer d1d2 = p2pHelper.Install (nodes.Get (1), nodes.Get (2));

  // Link n2-n3 (Bottleneck): 1Mbps, 50ms
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("50ms"));
  NetDeviceContainer d2d3 = p2pHelper.Install (nodes.Get (2), nodes.Get (3));

  // Link n3-n4: 10Mbps, 10ms
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer d3d4 = p2pHelper.Install (nodes.Get (3), nodes.Get (4));

  // Link n4-n5: 10Mbps, 10ms
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer d4d5 = p2pHelper.Install (nodes.Get (4), nodes.Get (5));

  // Assign IP Addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  ipv4.Assign (d0d1);

  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  ipv4.Assign (d1d2);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign (d2d3); // n2's IP is i2i3.GetAddress(0), n3's IP is i2i3.GetAddress(1)

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  ipv4.Assign (d3d4);

  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  ipv4.Assign (d4d5);

  // Enable static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TCP Flow: n2 (sender) to n3 (receiver)
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (i2i3.GetAddress (1), port)); // n3's IP address and port

  // Packet Sink on n3
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));

  // OnOff Application on n2
  OnOffHelper onoffHelper ("ns3::TcpSocketFactory", sinkAddress);
  onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // Always on
  onoffHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));                     // Send more than bottleneck
  onoffHelper.SetAttribute ("PacketSize", UintegerValue (1460));                                   // Standard MTU size - IP/TCP headers
  ApplicationContainer clientApps = onoffHelper.Install (nodes.Get (2));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (simulationTime));

  // Tracing configuration
  // Prepare output files for Cwnd and Pacing Rate
  AsciiTraceHelper asciiTraceHelper;
  g_cwndStream = asciiTraceHelper.CreateFileStream (outputFileName + "-cwnd.dat");
  g_pacingRateStream = asciiTraceHelper.CreateFileStream (outputFileName + "-pacingrate.dat");

  // Connect trace sources to the specific TCP socket
  // The OnOffApplication is installed on node 2, and it's the first application there.
  // The Tx/Socket trace source is the one we want.
  Config::ConnectWithoutContext ("/NodeList/2/ApplicationList/0/$ns3::OnOffApplication/Tx/Socket/CongestionWindow",
                                 MakeCallback (&CwndChange));
  Config::ConnectWithoutContext ("/NodeList/2/ApplicationList/0/$ns3::OnOffApplication/Tx/Socket/PacingRate",
                                 MakeCallback (&PacingRateChange));

  // Enable overall ASCII traces for packet transmission/reception times (.tr file)
  p2pHelper.EnableAsciiAll (asciiTraceHelper.CreateFileStream (outputFileName + ".tr"));

  // Flow Monitor
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Post-simulation analysis: print flow statistics and generate XML
  // The 'true' arguments enable printing flow statistics and per-flow statistics in the XML.
  flowMonitor->SerializeToXmlFile (outputFileName + ".xml", true, true);

  Simulator::Destroy ();

  return 0;
}