#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourNodeCustomTopology");

uint32_t g_totalRx = 0;

void RxCallback (Ptr<const Packet> packet, const Address &address)
{
  g_totalRx += packet->GetSize ();
}

int main (int argc, char *argv[])
{
  double metric = 1.0; // Default OSPF-like metric for n1-n3 link, configurable
  CommandLine cmd;
  cmd.AddValue ("metric", "Routing metric for n1 to n3 link (higher = less preferred)", metric);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4); // n0, n1, n2, n3

  // Point-to-Point links
  // n0 <-> n2
  NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2));
  // n1 <-> n2
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  // n2 <-> n3 (alternate path)
  NodeContainer n2n3 = NodeContainer (nodes.Get (2), nodes.Get (3));
  // n1 <-> n3 (direct high-cost path)
  NodeContainer n1n3 = NodeContainer (nodes.Get (1), nodes.Get (3));

  // Link helpers
  PointToPointHelper p2p_5mb2ms;
  p2p_5mb2ms.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_5mb2ms.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper p2p_15mb100ms;
  p2p_15mb100ms.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p_15mb100ms.SetChannelAttribute ("Delay", StringValue ("100ms"));

  PointToPointHelper p2p_15mb10ms;
  p2p_15mb10ms.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p_15mb10ms.SetChannelAttribute ("Delay", StringValue ("10ms"));

  // Install devices and channels
  NetDeviceContainer d_n0n2 = p2p_5mb2ms.Install (n0n2);
  NetDeviceContainer d_n1n2 = p2p_5mb2ms.Install (n1n2);
  NetDeviceContainer d_n1n3 = p2p_15mb100ms.Install (n1n3);
  NetDeviceContainer d_n2n3 = p2p_15mb10ms.Install (n2n3);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n0n2 = ipv4.Assign (d_n0n2);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n1n2 = ipv4.Assign (d_n1n2);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n1n3 = ipv4.Assign (d_n1n3);

  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n2n3 = ipv4.Assign (d_n2n3);

  // Set routing metrics (cost) -- Global Routing doesn't use metrics natively
  // So we will use static routing to make the n1-n3 link higher cost
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ptr<Ipv4> ipv4_n1 = nodes.Get (1)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n3 = nodes.Get (3)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> srt_n1 = staticRoutingHelper.GetStaticRouting (ipv4_n1);
  Ptr<Ipv4StaticRouting> srt_n3 = staticRoutingHelper.GetStaticRouting (ipv4_n3);

  // Routes from n1 to n3
  Ipv4Address dest = Ipv4Address ("10.0.3.2"); // n3's interface on n1n3
  Ipv4Address mask = Ipv4Mask ("255.255.255.255");

  // Remove any existing routes
  for (uint32_t i = 0; i < srt_n1->GetNRoutes (); ++i)
    {
      srt_n1->RemoveRoute (0);
    }
  // Add route via n2 (preferred)
  srt_n1->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"),
                              i_n1n2.GetAddress (1), 1); // via n2

  // Add direct route to n3 with configurable metric (simulate higher cost)
  // In static routing, we cannot set metric--so we can add it as a host route with
  // higher priority if metric is set low, else later in table if high.
  if (metric <= 1.0)
    {
      srt_n1->AddHostRouteTo (dest, i_n1n3.GetAddress (0), 2);
    }
  else
    {
      srt_n1->AddHostRouteTo (dest, i_n1n3.GetAddress (0), 100);
    }

  // (Optionally adjust n3's routing as well for return path)
  for (uint32_t i = 0; i < srt_n3->GetNRoutes (); ++i)
    {
      srt_n3->RemoveRoute (0);
    }
  srt_n3->AddNetworkRouteTo (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
                              i_n2n3.GetAddress (0), 1); // via n2
  srt_n3->AddHostRouteTo (i_n1n3.GetAddress (1), i_n1n3.GetAddress (1), 100);

  // Enable trace on queues
  p2p_5mb2ms.EnablePcapAll ("fournode-5mb2ms", false);
  p2p_15mb100ms.EnablePcapAll ("fournode-15mb100ms", false);
  p2p_15mb10ms.EnablePcapAll ("fournode-15mb10ms", false);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("fournode-queues.tr");
  p2p_5mb2ms.EnableAsciiAll (stream);
  p2p_15mb100ms.EnableAsciiAll (stream);
  p2p_15mb10ms.EnableAsciiAll (stream);

  // Set up UDP traffic from n3 (src) to n1 (dst)
  uint16_t port = 50000;

  // Create UDP server (sink) on n1
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApps = udpServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Trace packet receptions at sink
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback (&RxCallback));

  // Create UDP client on n3 targeting n1
  UdpClientHelper udpClient (i_n1n2.GetAddress (0), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (320));
  udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (50))); // 20 packets/second
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = udpClient.Install (nodes.Get (3));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  // Log results
  std::ofstream logFile;
  logFile.open ("simulation-results.log", std::ios::out | std::ios::trunc);
  logFile << "Total UDP packets received at n1: "
          << g_totalRx / 1024 << " kB" << std::endl;

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto itr = stats.begin (); itr != stats.end (); ++itr)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (itr->first);
      logFile << "Flow " << itr->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      logFile << "  Tx Packets: " << itr->second.txPackets << "\n";
      logFile << "  Rx Packets: " << itr->second.rxPackets << "\n";
      logFile << "  Lost Packets: " << itr->second.lostPackets << "\n";
      logFile << "  Throughput: " << (itr->second.rxBytes * 8.0 / (itr->second.timeLastRxPacket.GetSeconds () - itr->second.timeFirstTxPacket.GetSeconds ()) / 1024) << " Kbps\n";
    }
  logFile.close ();

  Simulator::Destroy ();
  return 0;
}