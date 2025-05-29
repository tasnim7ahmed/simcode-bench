#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211acSimulation");

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nPackets = 1000;
  double interval = 0.0001;
  std::string dataRate = "50Mbps";
  double simulationTime = 10;
  double distance = 10;
  bool rtsCtsEnabled = false;
  bool enableAsciiTrace = false;
  std::string transportProtocol = "UDP";
  uint32_t port = 9;
  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  NqosWifiMacHelper macHelper = NqosWifiMacHelper::Default ();
  std::string phyMode ("VhtMcs9"); // Default PHY rate
  bool shortGuardInterval = true;

  // Configure command line arguments
  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("nPackets", "Number of packets generated", nPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("dataRate", "Data rate for OnOff application", dataRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCtsEnabled);
  cmd.AddValue ("asciiTrace", "Enable Ascii tracing", enableAsciiTrace);
  cmd.AddValue ("transportProtocol", "Transport protocol (UDP or TCP)", transportProtocol);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("shortGuardInterval", "Enable short guard interval (SGI)", shortGuardInterval);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Wifi80211acSimulation", LOG_LEVEL_INFO);
    }

  // Disable fragmentation (Fragmentation doesn't work well with 802.11ac)
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  // Configure the channel width
  Config::SetDefault ("ns3::WifiChannel::Frequency", DoubleValue (5.180e+09)); // 5 GHz UNII-1 band
  Config::SetDefault ("ns3::WifiChannel::ChannelWidth", UintegerValue (80)); // 80 MHz channel
  Config::SetDefault ("ns3::WifiChannel::SecondChannelOffset", EnumValue (WifiChannel::ChannelOffset::NO_ADJACENT_CHANNEL));

  // Set guard interval
  if (shortGuardInterval)
  {
    Config::SetDefault("ns3::VhtWifiMac::ShortGuardIntervalSupported", BooleanValue(true));
  } else {
    Config::SetDefault("ns3::VhtWifiMac::ShortGuardIntervalSupported", BooleanValue(false));
  }

  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  // Configure the PHY layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelNumber", UintegerValue (36)); //Choose channel 36 which corresponds to 5180 MHz.

  // Configure the MAC layer
  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (Ssid ("ns-3-ssid")));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (phy, macHelper, staNode);

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (Ssid ("ns-3-ssid")),
                     "BeaconInterval", TimeValue (Seconds (0.05)));

  NetDeviceContainer apDevices;
  apDevices = wifiHelper.Install (phy, macHelper, apNode);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNode);
  mobility.Install (apNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure applications
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  if (transportProtocol == "UDP")
    {
      UdpServerHelper server (port);
      serverApps = server.Install (apNode);
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime + 1));

      UdpClientHelper client (apInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (nPackets));
      client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      clientApps = client.Install (staNode);
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));
    }
  else // TCP
    {
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
      serverApps = sink.Install (apNode.Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime + 1));

      BulkSendHelper client ("ns3::TcpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
      client.SetAttribute ("MaxBytes", UintegerValue (nPackets * 1024));
      clientApps = client.Install (staNode.Get (0));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  // Set the PHY mode after the devices are configured
  std::stringstream ss;
  ss << "HeMcs" << phyMode;
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue (phyMode),
                                        "ControlMode", StringValue (phyMode));

  // Enable RTS/CTS
  if (rtsCtsEnabled)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

  // Enable checksum
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  // Tracing
  if (enableAsciiTrace)
    {
      AsciiTraceHelper ascii;
      phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-80211ac.tr"));
    }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run the simulation
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // Analyze the results
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double goodput = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apInterface.GetAddress (0))
        {
          goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;
          std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << goodput << " Mbps\n";
          break;
        }
    }

  Simulator::Destroy ();

  return 0;
}