#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/energy-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterference");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  bool pcapTracing = false;
  double simulationTime = 10.0;
  double distance = 10.0;
  uint32_t mcsIndex = 7;
  uint32_t channelWidthIndex = 0;
  uint32_t guardIntervalIndex = 0;
  double waveformPower = 0.0;
  std::string wifiType = "ac";
  std::string errorModelType = "none";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("tracing", "Enable PCAP tracing.", tracing);
  cmd.AddValue ("pcapTracing", "Enable PCAP tracing.", pcapTracing);
  cmd.AddValue ("simulationTime", "Simulation duration (seconds).", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes (meters).", distance);
  cmd.AddValue ("mcsIndex", "MCS index (0-11).", mcsIndex);
  cmd.AddValue ("channelWidthIndex", "Channel Width index (0-3: 20MHz, 40MHz, 80MHz, 160MHz).", channelWidthIndex);
  cmd.AddValue ("guardIntervalIndex", "Guard Interval index (0-1: short, long).", guardIntervalIndex);
  cmd.AddValue ("waveformPower", "Waveform Power (dBm).", waveformPower);
  cmd.AddValue ("wifiType", "Wifi standard (a, b, g, n, ac, ax, be).", wifiType);
  cmd.AddValue ("errorModelType", "Error Model (none, RateErrorModel).", errorModelType);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiInterference", LOG_LEVEL_ALL);
    }

  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  NodeContainer interferenceNode;
  interferenceNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();

  phy.SetChannel (channel.Create ());
  phy.Set ("AntennaGain", DoubleValue (0));
  phy.Set ("TxGain", DoubleValue (10));

  WifiHelper wifi;

  std::string wifiStandard;
  if (wifiType == "a") wifiStandard = "OfdmRate54Mbps";
  else if (wifiType == "b") wifiStandard = "DsssRate11Mbps";
  else if (wifiType == "g") wifiStandard = "OfdmRate54Mbps";
  else if (wifiType == "n") wifiStandard = "HtMcs7";
  else if (wifiType == "ac") wifiStandard = "VhtMcs9";
  else if (wifiType == "ax") wifiStandard = "HeMcs11";
  else if (wifiType == "be") wifiStandard = "EhtMcs11";
  else wifiStandard = "VhtMcs9";

  wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiNodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiNodes.Get (0));

  // Install interference device
  NetDeviceContainer interferenceDevice;
  interferenceDevice = wifi.Install (phy, mac, interferenceNode.Get (0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);
  mobility.Install (interferenceNode);
  Ptr<ConstantPositionMobilityModel> interferenceMobility = interferenceNode.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  interferenceMobility->SetPosition (Vector(distance/2, distance/2, 0));

  InternetStackHelper stack;
  stack.Install (wifiNodes);
  stack.Install (interferenceNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer interferenceInterface = address.Assign (interferenceDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 5000;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (wifiNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (wifiNodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (apInterfaces.GetAddress (0), 9));
  source.SetAttribute ("SendSize", UintegerValue (1024));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (wifiNodes.Get (1));
  sourceApps.Start (Seconds (3.0));
  sourceApps.Stop (Seconds (simulationTime - 3));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApps = sink.Install (wifiNodes.Get (0));
  sinkApps.Start (Seconds (2.0));
  sinkApps.Stop (Seconds (simulationTime - 2));

  // Configure interference source (UDP)
  uint16_t interferencePort = 6000;
  UdpClientHelper interferenceSource (apInterfaces.GetAddress (0), interferencePort);
  interferenceSource.SetAttribute ("MaxPackets", UintegerValue (1000000));
  interferenceSource.SetAttribute ("Interval", TimeValue (Seconds (0.0005)));
  interferenceSource.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer interferenceApps = interferenceSource.Install (interferenceNode.Get (0));
  interferenceApps.Start (Seconds (2.5));
  interferenceApps.Stop (Seconds (simulationTime - 2.5));

  PacketSinkHelper interferenceSink ("ns3::UdpSocketFactory",
      InetSocketAddress (Ipv4Address::GetAny (), interferencePort));
  ApplicationContainer interferenceSinkApps = interferenceSink.Install (wifiNodes.Get(0));
  interferenceSinkApps.Start (Seconds (2.0));
  interferenceSinkApps.Stop (Seconds (simulationTime -2));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (pcapTracing == true)
    {
      phy.EnablePcapAll ("wifi-interference");
    }

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("wifi-interference.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}