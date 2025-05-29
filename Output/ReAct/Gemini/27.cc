#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiPerformance");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 1;
  double simulationTime = 10;
  std::string dataRate = "5Mbps";
  std::string phyMode = "OfdmRate6MbpsBW20MHz";
  double distance = 10.0;
  bool rtsCtsEnabled = false;
  std::string transportProtocol = "Udp";
  uint32_t packetSize = 1472;
  uint32_t port = 9;
  double expectedThroughput = 5; // Mbps
  int mcs = 0;
  int channelWidth = 20;
  std::string guardInterval = "Long";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("dataRate", "Data rate for UDP traffic", dataRate);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCtsEnabled);
  cmd.AddValue ("transportProtocol", "Transport protocol (Udp or Tcp)", transportProtocol);
  cmd.AddValue ("packetSize", "Size of packets", packetSize);
  cmd.AddValue ("expectedThroughput", "Expected throughput (Mbps)", expectedThroughput);
  cmd.AddValue ("mcs", "MCS value (0-7)", mcs);
  cmd.AddValue ("channelWidth", "Channel width (20 or 40)", channelWidth);
  cmd.AddValue ("guardInterval", "Guard interval (Short or Long)", guardInterval);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiPerformance", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns-3-ssid")));
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  if (rtsCtsEnabled)
    {
      Config::SetDefault ("ns3::WifiMac::RtsCtsThreshold", StringValue ("0"));
    }

  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, NodeContainer ());

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, NodeContainer ());

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  NetDeviceContainer staDevs = wifi.Install (phy, mac, wifiStaNodes);
  NetDeviceContainer apDevs = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiApNode);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  Ptr<ConstantPositionMobilityModel> apMobility = wifiApNode.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  apMobility->SetPosition (Vector (0.0, 0.0, 0.0));

  for (uint32_t i = 0; i < nWifi; ++i)
    {
      Ptr<ConstantPositionMobilityModel> staMobility = wifiStaNodes.Get (i)->GetObject<ConstantPositionMobilityModel> ();
      staMobility->SetPosition (Vector (distance, 0.0, 0.0));
    }

  InternetStackHelper internet;
  internet.Install (wifiApNode);
  internet.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;

  staNodeInterface = address.Assign (staDevs);
  apNodeInterface = address.Assign (apDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  ApplicationContainer sourceApps;
  ApplicationContainer sinkApps;

  if (transportProtocol == "Udp")
    {
      UdpEchoServerHelper echoServer (port);
      sinkApps = echoServer.Install (wifiApNode.Get (0));
      sinkApps.Start (Seconds (0.0));
      sinkApps.Stop (Seconds (simulationTime + 1));

      UdpEchoClientHelper echoClient (apNodeInterface.GetAddress (0), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)(simulationTime * (expectedThroughput * 1e6) / (packetSize * 8)))); // Adjust MaxPackets
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (packetSize * 8 / (double)(expectedThroughput * 1e6))));        // Adjust Interval
      echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      sourceApps = echoClient.Install (wifiStaNodes.Get (0));
      sourceApps.Start (Seconds (1.0));
      sourceApps.Stop (Seconds (simulationTime));
    }
  else if (transportProtocol == "Tcp")
    {
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
      sinkApps = packetSinkHelper.Install (wifiApNode.Get (0));
      sinkApps.Start (Seconds (0.0));
      sinkApps.Stop (Seconds (simulationTime + 1));

      BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", InetSocketAddress (apNodeInterface.GetAddress (0), port));
      bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
      sourceApps = bulkSendHelper.Install (wifiStaNodes.Get (0));
      sourceApps.Start (Seconds (1.0));
      sourceApps.Stop (Seconds (simulationTime));
    }
  else
    {
      std::cerr << "Error: Invalid transport protocol. Choose 'Udp' or 'Tcp'." << std::endl;
      return 1;
    }

  phy.Set ("Mcs", IntegerValue (mcs));

  Config::Set ("/ChannelList/0/ChannelWidth", UintegerValue (channelWidth));
  if (guardInterval == "Short") {
    Config::SetDefault ("ns3::HtWifiMac::ShortGuardIntervalSupported", BooleanValue(true));
  } else {
    Config::SetDefault ("ns3::HtWifiMac::ShortGuardIntervalSupported", BooleanValue(false));
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double throughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apNodeInterface.GetAddress (0))
        {
          throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Throughput: " << throughput << " Mbps\n";
          break;
        }
    }

  Simulator::Destroy ();

  return 0;
}