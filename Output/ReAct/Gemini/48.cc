#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  bool useSpectrum = false;
  std::string phyMode ("HtMcs7");
  double simulationTime = 10;
  double distance = 10;
  bool useUdp = true;
  int mcsIndex = 7;
  int channelWidth = 20;
  bool useShortGuardInterval = false;
  std::string dataRate = "54Mbps";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("useSpectrum", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrum);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes in meters", distance);
  cmd.AddValue ("useUdp", "Use UDP instead of TCP", useUdp);
  cmd.AddValue ("mcsIndex", "MCS index for 802.11n", mcsIndex);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("useShortGuardInterval", "Use short guard interval", useShortGuardInterval);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("65535"));

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelay");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  if (useSpectrum)
    {
      phy = SpectrumWifiPhyHelper ();
    }

  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (phyMode),
                                      "ControlMode", StringValue (phyMode));

  Ssid ssid = Ssid ("ns-3-ssid");
  WifiMacHelper mac;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (1.0)),
               "QosSupported", BooleanValue (true));

  NetDeviceContainer apDevices;
  apDevices = wifiHelper.Install (phy, mac, nodes.Get (0));

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (apDevices);
  address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  ApplicationContainer app;
  if (useUdp)
    {
      UdpClientHelper client (interfaces.GetAddress (1), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      client.SetAttribute ("Interval", TimeValue (Time ("0.00002")));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      app = client.Install (nodes.Get (0));

      UdpServerHelper server (port);
      app.Add (server.Install (nodes.Get (1)));
    }
  else
    {
      BulkSendHelper source ("ns3::TcpSocketFactory",
                             InetSocketAddress (interfaces.GetAddress (1), port));
      source.SetAttribute ("SendSize", UintegerValue (1024));
      app = source.Install (nodes.Get (0));

      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      app.Add (sink.Install (nodes.Get (1)));
    }

  app.Start (Seconds (1.0));
  app.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (enablePcap)
    {
      phy.EnablePcap ("wifi-simulation", apDevices);
    }

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalRxBytes = 0;
  double totalTxBytes = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
      totalRxBytes += i->second.rxBytes;
      totalTxBytes += i->second.txBytes;
    }

  std::cout << "Total throughput: " << totalRxBytes * 8.0 / simulationTime / 1000000 << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}