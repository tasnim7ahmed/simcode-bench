#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMixedNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  std::string phyMode ("DsssRate1Mbps");
  double simulationTime = 10; // seconds
  uint32_t packetSize = 1472; // bytes
  std::string dataRate ("50Mbps");
  std::string tcpVariant ("TcpNewReno");

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue ("dataRate", "Application data rate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                  "TcpLinuxReno, TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, "
                  "TcpScalable, TcpVeno", tcpVariant);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);

  cmd.Parse (argc, argv);

  // Set up TCP congestion control algorithm
  if (tcpVariant.compare ("TcpNewReno") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    }
  else if (tcpVariant.compare ("TcpLinuxReno") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpLinuxReno"));
    }
  else
    {
      NS_LOG_ERROR ("Invalid TCP variant: " << tcpVariant);
      exit (1);
    }

  // Configure Wifi
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Configure MAC layer
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  Ssid ssid = Ssid ("ns-3-ssid");

  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  NodeContainer staNodes;
  staNodes.Create (2);
  staDevices = wifi.Install (wifiPhy, wifiMac, staNodes);

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  NodeContainer apNode;
  apNode.Create (1);
  apDevices = wifi.Install (wifiPhy, wifiMac, apNode);

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
  mobility.Install (staNodes);
  mobility.Install (apNode);

  // Enable b/g protection
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonErpPresent", BooleanValue (true));

  // Set operating rate
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrcRetries", StringValue ("7"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSsrcRetries", StringValue ("7"));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Define traffic flow: Sta0 -> Ap, Sta1 -> Ap

  // Install UDP Application
  uint16_t port = 9; // Discard port (RFC 863)
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpEchoClientHelper echoClient (apInterface.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.00002))); //packets/s
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps;
  clientApps.Add (echoClient.Install (staNodes.Get (0)));
  clientApps.Add (echoClient.Install (staNodes.Get (1)));

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  // Install TCP Application
  uint16_t sinkPort = 8080;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (apNode.Get (0));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (simulationTime + 1));

  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory",
                                 InetSocketAddress (apInterface.GetAddress (0), sinkPort));
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  bulkSendHelper.SetAttribute ("SendSize", UintegerValue (packetSize));

  ApplicationContainer sourceApps;
  sourceApps.Add (bulkSendHelper.Install (staNodes.Get (0)));
  sourceApps.Add (bulkSendHelper.Install (staNodes.Get (1)));

  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (simulationTime + 1));

  // Enable Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Set up tracing
  if (tracing == true)
    {
      wifiPhy.EnablePcapAll ("wifi-mixed-network");
    }

  // Run simulation
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // Print Flow Monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}