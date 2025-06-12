#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWifiNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  std::string phyMode ("OfdmRate6Mbps");
  uint32_t payloadSize = 1472; // bytes
  std::string dataRate ("5Mbps");
  std::string bottleNeckLinkDataRate ("5Mbps");
  std::string bottleNeckLinkDelay ("0ms");
  uint32_t numStaB = 1;
  uint32_t numStaG = 1;
  uint32_t numStaHT = 1;
  uint32_t numAp = 1;
  std::string protocol ("Udp"); // or Tcp
  bool enableErpProtection = false;
  bool useShortPreamble = false;
  bool useShortSlotTime = false;

  // command line parameters
  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data rate", dataRate);
  cmd.AddValue ("bottleNeckLinkDataRate", "BottleNeck link data rate", bottleNeckLinkDataRate);
  cmd.AddValue ("bottleNeckLinkDelay", "BottleNeck link delay", bottleNeckLinkDelay);
  cmd.AddValue ("numStaB", "Number of b stations", numStaB);
  cmd.AddValue ("numStaG", "Number of g stations", numStaG);
  cmd.AddValue ("numStaHT", "Number of HT stations", numStaHT);
  cmd.AddValue ("numAp", "Number of AP", numAp);
  cmd.AddValue ("protocol", "Transport protocol to use: Udp or Tcp", protocol);
  cmd.AddValue ("enableErpProtection", "Enable ERP protection (0 or 1)", enableErpProtection);
  cmd.AddValue ("useShortPreamble", "Use short preamble (0 or 1)", useShortPreamble);
  cmd.AddValue ("useShortSlotTime", "Use short slot time (0 or 1)", useShortSlotTime);
  cmd.Parse (argc, argv);

  // default setup values
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (payloadSize));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (dataRate));

  // set non-QoS EDCA parameters
  Config::SetDefault ("ns3::WifiMacQueue::MaxPackets", UintegerValue (100));
  if (verbose)
    {
      LogComponentEnable ("MixedWifiNetwork", LOG_LEVEL_INFO);
    }

  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

  NodeContainer apNodes;
  apNodes.Create (numAp);

  NodeContainer staNodesB;
  staNodesB.Create (numStaB);

  NodeContainer staNodesG;
  staNodesG.Create (numStaG);

  NodeContainer staNodesHT;
  staNodesHT.Create (numStaHT);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  // This is one parameter that matters when using FixedRssLossModel
  // set it to the transmit power
  wifiPhy.Set ("TxPowerStart", DoubleValue (33));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (33));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate adaptation
  WifiMacHelper wifiMac;

  Ssid ssid = Ssid ("mixed-wifi");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconInterval", TimeValue (Seconds (0.05)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhy, wifiMac, apNodes);

  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevicesB;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  staDevicesB = wifi.Install (wifiPhy, wifiMac, staNodesB);

  NetDeviceContainer staDevicesG;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  staDevicesG = wifi.Install (wifiPhy, wifiMac, staNodesG);

  NetDeviceContainer staDevicesHT;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);
  staDevicesHT = wifi.Install (wifiPhy, wifiMac, staNodesHT);

  // Mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (apNodes);
  mobility.Install (staNodesB);
  mobility.Install (staNodesG);
  mobility.Install (staNodesHT);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodesB);
  stack.Install (staNodesG);
  stack.Install (staNodesHT);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfacesB = address.Assign (staDevicesB);
  Ipv4InterfaceContainer staInterfacesG = address.Assign (staDevicesG);
  Ipv4InterfaceContainer staInterfacesHT = address.Assign (staDevicesHT);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Applications

  uint16_t port = 9;  // Discard port (RFC 863)

  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < numStaB; ++i)
    {
      PacketSinkHelper sink (protocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                             InetSocketAddress (staInterfacesB.GetAddress (i), port));
      sinkApps.Add (sink.Install (staNodesB.Get (i)));
    }

  for (uint32_t i = 0; i < numStaG; ++i)
    {
      PacketSinkHelper sink (protocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                             InetSocketAddress (staInterfacesG.GetAddress (i), port));
      sinkApps.Add (sink.Install (staNodesG.Get (i)));
    }

  for (uint32_t i = 0; i < numStaHT; ++i)
    {
      PacketSinkHelper sink (protocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                             InetSocketAddress (staInterfacesHT.GetAddress (i), port));
      sinkApps.Add (sink.Install (staNodesHT.Get (i)));
    }

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numStaB; ++i)
    {
      OnOffHelper onoff (protocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                         InetSocketAddress (staInterfacesB.GetAddress (i), port));
      clientApps.Add (onoff.Install (apNodes.Get (0)));
    }

   for (uint32_t i = 0; i < numStaG; ++i)
    {
      OnOffHelper onoff (protocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                         InetSocketAddress (staInterfacesG.GetAddress (i), port));
      clientApps.Add (onoff.Install (apNodes.Get (0)));
    }

  for (uint32_t i = 0; i < numStaHT; ++i)
    {
      OnOffHelper onoff (protocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                         InetSocketAddress (staInterfacesHT.GetAddress (i), port));
      clientApps.Add (onoff.Install (apNodes.Get (0)));
    }

  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  if (tracing)
    {
      wifiPhy.EnablePcapAll ("mixed-wifi");
    }

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalRxBytes = 0;

  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      // Consider only flows from AP to stations
      if (t.sourceAddress == apInterfaces.GetAddress (0))
        {
          totalRxBytes += i->second.rxBytes;

          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps\n";
        }
    }

  double totalThroughput = totalRxBytes * 8.0 / (Simulator::Now ().GetSeconds () - 1.0) / 1024 / 1024;
  std::cout << "\nTotal Throughput: " << totalThroughput << " Mbps\n";

  monitor->SerializeToXmlFile("mixed-wifi.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}