#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/log.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHiddenStationAggregation");

int main (int argc, char *argv[])
{
  bool rtsCtsEnabled = false;
  uint32_t maxAmpduSize = 64;
  uint32_t payloadSize = 1472;
  double simulationTime = 10;
  double lowThrouputBound = 50;
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCtsEnabled);
  cmd.AddValue ("maxAmpduSize", "Maximum A-MPDU size", maxAmpduSize);
  cmd.AddValue ("payloadSize", "Payload size", payloadSize);
  cmd.AddValue ("simulationTime", "Simulation time", simulationTime);
  cmd.AddValue ("lowThrouputBound", "Low throughput bound", lowThrouputBound);
  cmd.AddValue ("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue (maxAmpduSize));

  NodeContainer apNode;
  apNode.Create (1);

  NodeContainer staNodes;
  staNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNodes);

  if (rtsCtsEnabled)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Minstrel/RtsCtsThreshold", StringValue ("0"));
    }

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  NodeContainer allNodes;
  allNodes.Add (apNode);
  allNodes.Add (staNodes);
  mobility.Install (allNodes);

  Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  apMobility->SetPosition (Vector (10, 10, 0));
  Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  sta1Mobility->SetPosition (Vector (0, 0, 0));
  Ptr<ConstantPositionMobilityModel> sta2Mobility = staNodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  sta2Mobility->SetPosition (Vector (0, 20, 0));

  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpClientHelper client1 (apInterface.GetAddress (0), port);
  client1.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client1.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  client1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  ApplicationContainer clientApps1 = client1.Install (staNodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client2 (apInterface.GetAddress (0), port);
  client2.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client2.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  client2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  ApplicationContainer clientApps2 = client2.Install (staNodes.Get (1));
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (simulationTime + 1));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (apNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  if (pcapTracing)
    {
      phy.EnablePcapAll ("wifi-hidden-station-aggregation");
    }

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-hidden-station-aggregation.tr"));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalRxBytes = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      totalRxBytes += i->second.rxBytes;
    }

  double throughput = totalRxBytes * 8.0 / (simulationTime * 1000000.0);

  std::cout << "Aggregate Throughput: " << throughput << " Mbps\n";

  if (throughput < lowThrouputBound)
    {
      std::cerr << "Validation failed.  Throughput " << throughput
                << " Mbps is below the threshold of " << lowThrouputBound << " Mbps\n";
      exit (1);
    }

  monitor->SerializeToXmlFile ("wifi-hidden-station-aggregation.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}