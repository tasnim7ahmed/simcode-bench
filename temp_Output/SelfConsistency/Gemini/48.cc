#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiPhyExperiment");

int main (int argc, char *argv[])
{
  bool verbose = false;
  double simulationTime = 10; // seconds
  double distance = 10;      // meters
  std::string phyMode ("OfdmRate6Mbps");
  uint32_t packetSize = 1472; // bytes
  uint32_t numPackets = 1000;
  bool useSpectrumPhy = false;
  bool useUdp = true;
  uint32_t mcs = 7; // MCS index (0-7 for 802.11n)
  uint32_t channelWidth = 20; // MHz
  std::string guardInterval = "800ns";
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("phyMode", "Wifi PHY mode", phyMode);
  cmd.AddValue ("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("useSpectrumPhy", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrumPhy);
  cmd.AddValue ("useUdp", "Use UDP instead of TCP", useUdp);
  cmd.AddValue ("mcs", "MCS index", mcs);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("guardInterval", "Guard interval (400ns or 800ns)", guardInterval);
  cmd.AddValue ("pcap", "Enable PCAP tracing", pcapTracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns-3-ssid")));

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  SpectrumWifiPhyHelper spectrumPhy;

  if(useSpectrumPhy) {
      phy = SpectrumWifiPhyHelper::Default();
  }

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansPropagationLossModel propagationLossModel;
  YansPropagationDelayModel delayModel;

  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  if (useSpectrumPhy) {
      spectrumPhy.SetChannel (channel.Create ());
      spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (guardInterval == "400ns"));
      spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
  } else {
      phy.SetChannel (channel.Create ());
      phy.Set ("ShortGuardEnabled", BooleanValue (guardInterval == "400ns"));
      phy.Set ("ChannelWidth", UintegerValue (channelWidth));

  }

  WifiMacHelper mac;
  NetDeviceContainer devices;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (Ssid ("ns-3-ssid")),
               "ActiveProbing", BooleanValue (false));

  devices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (Ssid ("ns-3-ssid")),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (2.5)));

  devices = wifi.Install (phy, mac, nodes.Get (0));

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
  Ipv4InterfaceContainer i = address.Assign (devices);

  uint16_t port = 9; // Discard port (RFC 863)

  ApplicationContainer app;
  if (useUdp)
    {
      UdpServerHelper echoServer (port);
      app = echoServer.Install (nodes.Get (1));
      app.Start (Seconds (1.0));
      app.Stop (Seconds (simulationTime + 1));

      UdpClientHelper echoClient (i.GetAddress (1), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01))); // packets/sec
      echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      app = echoClient.Install (nodes.Get (0));
      app.Start (Seconds (2.0));
      app.Stop (Seconds (simulationTime + 1));
    }
  else
    {
      // TCP traffic
      uint16_t sinkPort = 8080;
      Address sinkAddress (InetSocketAddress (i.GetAddress (1), sinkPort));
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
      ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
      sinkApps.Start (Seconds (1.0));
      sinkApps.Stop (Seconds (simulationTime + 1));

      BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sinkAddress);
      bulkSendHelper.SetAttribute ("SendSize", UintegerValue (packetSize));
      bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (numPackets * packetSize));
      ApplicationContainer sourceApps = bulkSendHelper.Install (nodes.Get (0));
      sourceApps.Start (Seconds (2.0));
      sourceApps.Stop (Seconds (simulationTime + 1));
    }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  if(pcapTracing) {
      phy.EnablePcap ("wifi-experiment", devices);
  }

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double throughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
      throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;

    }

  monitor->SerializeToXmlFile("wifi-experiment.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}