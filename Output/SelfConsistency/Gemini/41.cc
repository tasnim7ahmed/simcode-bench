#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridOlsrExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  std::string phyMode ("DsssRate11Mbps");
  double distance = 1.0;  // Grid distance
  uint32_t packetSize = 1000; // Packet size in bytes
  uint32_t numPackets = 1; // Number of packets to send
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  bool tracing = false;
  bool enableAsciiTrace = false;

  // Allow command-line options to be set, as well as command-line
  // parameters for PacketSink applications.
  CommandLine cmd;
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "Grid distance", distance);
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("sinkNode", "Node which is the sink", sinkNode);
  cmd.AddValue ("sourceNode", "Node which is the source", sourceNode);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("ascii", "Enable Ascii tracing", enableAsciiTrace);
  cmd.Parse (argc, argv);

  // Fix non-QoS fragmentation problems when using higher rates
  AmsduAggregationParameter::Set ("MaxAmsduSize", UintegerValue (3839));

  // Set up Wifi
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on everything
    }
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // This is one parameter that matters when using FixedRssLossModel
  // set it to zero; otherwise, our transmission range will exceed the
  // bounding box
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("Model", "ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("Model", "ns3::FixedRssLossModel",
                                  "Rss", DoubleValue (-80)); // Adjust RSSI cutoff as needed
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, NodeContainer ());

  // Create Nodes
  NodeContainer nodes;
  nodes.Create (25);

  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // OLSR
  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Create OnOff application to send UDP datagrams from node sourceNode to node sinkNode.
  uint16_t port = 9;   // Discard port (RFC 863)

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (i.GetAddress (sinkNode), port)));
  onoff.SetConstantRate (DataRate ("500kbps"));

  ApplicationContainer app = onoff.Install (nodes.Get (sourceNode));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (sinkNode));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));


  // Tracing
  if (tracing)
    {
      wifiPhy.EnablePcap ("adhoc-grid-olsr", devices);
    }

  if (enableAsciiTrace)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("adhoc-grid-olsr.tr"));
    }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // NetAnim
  AnimationInterface anim ("adhoc-grid-olsr.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      anim.UpdateNodeColor (nodes.Get (i), 0, 255, 0); // Green for all nodes
    }
  anim.UpdateNodeColor (nodes.Get (sourceNode), 255, 0, 0); // Red for source
  anim.UpdateNodeColor (nodes.Get (sinkNode), 0, 0, 255);    // Blue for sink


  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("adhoc-grid-olsr.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}