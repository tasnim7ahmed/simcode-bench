#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiPhyComparison");

int main (int argc, char *argv[])
{
  bool useSpectrumWifiPhy = false;
  bool useUdp = true;
  bool pcapTracing = false;
  double simulationTime = 10.0;
  double distance = 10.0;
  int mcsIndex = 7; // Example MCS index
  int channelWidth = 20; // Channel width in MHz
  bool useShortGuardInterval = false;

  CommandLine cmd;
  cmd.AddValue ("useSpectrumWifiPhy", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrumWifiPhy);
  cmd.AddValue ("useUdp", "Use UDP instead of TCP", useUdp);
  cmd.AddValue ("pcapTracing", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes in meters", distance);
  cmd.AddValue ("mcsIndex", "MCS index", mcsIndex);
  cmd.AddValue ("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue ("useShortGuardInterval", "Use short guard interval", useShortGuardInterval);

  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("DsssRate1Mbps"));

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();

  if (useSpectrumWifiPhy)
    {
      phy = SpectrumWifiPhyHelper::Default ();
    }
  else
    {
      phy = YansWifiPhyHelper::Default ();
    }

  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (0));

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (staDevices);
  interfaces = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  ApplicationContainer app;
  if (useUdp)
    {
      UdpEchoServerHelper echoServer (port);
      app = echoServer.Install (nodes.Get (0));

      UdpEchoClientHelper echoClient (interfaces.GetAddress (0), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      app.Add (echoClient.Install (nodes.Get (1)));
    }
  else
    {
      BulkSendHelper source ("ns3::TcpSocketFactory",
                             InetSocketAddress (interfaces.GetAddress (0), port));
      source.SetAttribute ("SendSize", UintegerValue (1024));
      app = source.Install (nodes.Get (1));

      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      app.Add (sink.Install (nodes.Get (0)));
    }

  app.Start (Seconds (1.0));
  app.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  phy.EnableMonitorSniffer ("wifi-phy-comparison.pcap", "wlan0", true, true);

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalRxBytes = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
      totalRxBytes += i->second.rxBytes;
    }

  double totalThroughput = totalRxBytes * 8.0 / simulationTime / 1024 / 1024;
  std::cout << "Total Throughput: " << totalThroughput << " Mbps\n";

  Simulator::Destroy ();
  return 0;
}