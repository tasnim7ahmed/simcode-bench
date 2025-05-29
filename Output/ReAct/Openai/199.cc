#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterLanTcpSimulation");

int main (int argc, char *argv[])
{
  // Simulation configuration
  double simTime = 10.0; // seconds
  std::string outputFileName = "tcp-results.txt";
  std::string dataRate = "10Mbps";
  std::string delay = "2ms";
  uint32_t lanNodes = 3;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create routers
  NodeContainer routers;
  routers.Create (3); // R1, R2, R3

  // Point-to-point links: R1<->R2 and R2<->R3
  NodeContainer r1r2 = NodeContainer (routers.Get (0), routers.Get (1));
  NodeContainer r2r3 = NodeContainer (routers.Get (1), routers.Get (2));

  // Create LAN nodes, including R2 as a member of the LAN
  NodeContainer lanNodesContainer;
  lanNodesContainer.Add (routers.Get (1)); // R2
  lanNodesContainer.Create (lanNodes); // Add LAN hosts

  // Assigning node indices for easier reference
  Ptr<Node> r3Host = CreateObject<Node> ();
  // To connect host to R3, create a point-to-point link
  NodeContainer r3HostContainer;
  r3HostContainer.Add (r3Host);
  r3HostContainer.Add (routers.Get (2));

  // Install internet stack
  InternetStackHelper internet;
  internet.Install (routers);
  internet.Install (lanNodesContainer.Get (1)); // First LAN host
  internet.Install (lanNodesContainer.Get (2));
  internet.Install (lanNodesContainer.Get (3));
  internet.Install (r3Host);

  // Configure point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer d_r1r2 = p2p.Install (r1r2);
  NetDeviceContainer d_r2r3 = p2p.Install (r2r3);

  // Connect R3 to its host
  NetDeviceContainer d_r3host = p2p.Install (r3HostContainer);

  // Configure LAN (CSMA)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer d_lan = csma.Install (lanNodesContainer);

  // Assign IPv4 addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_r1r2 = ipv4.Assign (d_r1r2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_r2r3 = ipv4.Assign (d_r2r3);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_lan = ipv4.Assign (d_lan);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i_r3host = ipv4.Assign (d_r3host);

  // Enable static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install TCP server (PacketSink) on R3's host
  uint16_t port = 5000;
  Address sinkAddress (InetSocketAddress (i_r3host.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (r3Host);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime));

  // Install TCP source (OnOffApplication) on first host in the LAN (not R2!)
  OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer clientApps = onoff.Install (lanNodesContainer.Get (1)); // First LAN Host

  // Enable Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Output statistics
  std::ofstream output (outputFileName);
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  output << "Flow\tSource\tDestination\tTxPackets\tRxPackets\tThroughput (kbps)\n";
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      double throughput = iter->second.rxBytes * 8.0 / (simTime - 1.0) / 1000; // exclude pre-app-start time
      output << iter->first << "\t"
             << t.sourceAddress << "\t"
             << t.destinationAddress << "\t"
             << iter->second.txPackets << "\t"
             << iter->second.rxPackets << "\t"
             << throughput << "\n";
    }
  output.close ();

  Simulator::Destroy ();
  return 0;
}