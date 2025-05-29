#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include "ns3/command-line.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterference");

int main (int argc, char *argv[])
{
  bool verbose = false;
  std::string phyMode ("DsssRate1Mbps");
  double primaryRss = -80; // dBm
  double interferingRss = -60; //dBm
  double timeOffset = 0.000001; // seconds
  uint32_t primaryPacketSize = 1000;
  uint32_t interferingPacketSize = 500;
  std::string pcapFile = "wifi-interference.pcap";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("primaryRss", "Rx RSS for primary signal (dBm)", primaryRss);
  cmd.AddValue ("interferingRss", "Rx RSS for interfering signal (dBm)", interferingRss);
  cmd.AddValue ("timeOffset", "Time offset between primary and interfering signals (seconds)", timeOffset);
  cmd.AddValue ("primaryPacketSize", "Size of primary packet", primaryPacketSize);
  cmd.AddValue ("interferingPacketSize", "Size of interfering packet", interferingPacketSize);
  cmd.AddValue ("pcapFile", "Name of pcap file", pcapFile);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("WifiInterference", LOG_LEVEL_ALL);
    }

  NodeContainer nodes;
  nodes.Create (3);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel", "Rss", DoubleValue (primaryRss));
  wifiChannel.AddPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Install mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (i.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (primaryPacketSize));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  // Interfering signal
  YansWifiChannelHelper wifiChannelInterference = YansWifiChannelHelper::Default ();
  wifiChannelInterference.AddPropagationLoss ("ns3::FixedRssLossModel", "Rss", DoubleValue (interferingRss));
  wifiChannelInterference.AddPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper wifiPhyInterference = YansWifiPhyHelper::Default ();
  wifiPhyInterference.SetChannel (wifiChannelInterference.Create ());

  WifiMacHelper wifiMacInterference;
  wifiMacInterference.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer interferingDevice = wifi.Install (wifiPhyInterference, wifiMacInterference, nodes.Get (2));

  UdpClientHelper clientInterference (i.GetAddress (1), 9);
  clientInterference.SetAttribute ("MaxPackets", UintegerValue (1));
  clientInterference.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientInterference.SetAttribute ("PacketSize", UintegerValue (interferingPacketSize));
  apps = clientInterference.Install (nodes.Get (2));
  apps.Start (Seconds (2.0 + timeOffset));
  apps.Stop (Seconds (10.0));

  wifiPhy.EnablePcap (pcapFile, devices);

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
	  std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();

  return 0;
}