#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpatialReuseSimulation");

int main (int argc, char *argv[])
{
  bool enableObssPd = false;
  std::string dataRate = "OfdmRate6MbpsBW20MHz";
  double simulationTime = 10.0;
  double distance1 = 10.0;
  double distance2 = 15.0;
  double distance3 = 20.0;
  double txPowerDbm = 20.0;
  int ccaEdThreshold = -82;
  int obssPdThreshold = -75;
  std::string channelWidth = "20MHz";
  uint32_t packetSize = 1024;
  std::string interval = "0.01"; //seconds

  CommandLine cmd;
  cmd.AddValue ("enableObssPd", "Enable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue ("dataRate", "Data rate for transmissions", dataRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance1", "Distance between STA1 and AP1", distance1);
  cmd.AddValue ("distance2", "Distance between STA2 and AP2", distance2);
  cmd.AddValue ("distance3", "Distance between AP1 and AP2", distance3);
  cmd.AddValue ("txPowerDbm", "Transmit power in dBm", txPowerDbm);
  cmd.AddValue ("ccaEdThreshold", "CCA ED threshold", ccaEdThreshold);
  cmd.AddValue ("obssPdThreshold", "OBSS PD threshold", obssPdThreshold);
  cmd.AddValue ("channelWidth", "Channel width (20MHz, 40MHz, 80MHz)", channelWidth);
  cmd.AddValue ("packetSize", "Size of the packets to send", packetSize);
  cmd.AddValue ("interval", "Interval between packets", interval);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (2);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211ax);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

  if (channelWidth == "20MHz")
    {
      wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    }
  else if (channelWidth == "40MHz")
    {
      wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5.2e9));
    }
  else if (channelWidth == "80MHz")
    {
      wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5.5e9));
    }
  else
    {
      std::cerr << "Invalid channel width.  Using 20MHz" << std::endl;
      channelWidth = "20MHz";
      wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    }

  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);

  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));

  WifiMacHelper wifiMac;

  Ssid ssid1 = Ssid ("ns-3-ssid-1");
  Ssid ssid2 = Ssid ("ns-3-ssid-2");

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid1),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevice1 = wifiHelper.Install (wifiPhy, wifiMac, apNodes.Get (0));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid2),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevice2 = wifiHelper.Install (wifiPhy, wifiMac, apNodes.Get (1));

  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid1),
                   "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice1 = wifiHelper.Install (wifiPhy, wifiMac, staNodes.Get (0));

    wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid2),
                   "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice2 = wifiHelper.Install (wifiPhy, wifiMac, staNodes.Get (1));


  if (enableObssPd)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ObssPdThreshold", IntegerValue (obssPdThreshold));
    }

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/CcaEdThreshold", IntegerValue (ccaEdThreshold));

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

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (distance1),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (staNodes.Get(0));

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (distance3 + distance2),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (staNodes.Get(1));

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface1 = address.Assign (apDevice1);
  Ipv4InterfaceContainer staInterface1 = address.Assign (staDevice1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface2 = address.Assign (apDevice2);
  Ipv4InterfaceContainer staInterface2 = address.Assign (staDevice2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper server1 (9);
  ApplicationContainer serverApps1 = server1.Install (apNodes.Get (0));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client1 (apInterface1.GetAddress (0), 9);
  client1.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client1.SetAttribute ("Interval", TimeValue (Seconds (std::stod(interval))));
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps1 = client1.Install (staNodes.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (simulationTime));

  UdpServerHelper server2 (9);
  ApplicationContainer serverApps2 = server2.Install (apNodes.Get (1));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client2 (apInterface2.GetAddress (0), 9);
  client2.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client2.SetAttribute ("Interval", TimeValue (Seconds (std::stod(interval))));
  client2.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps2 = client2.Install (staNodes.Get (1));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (simulationTime));


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalThroughput1 = 0.0;
  double totalThroughput2 = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      if (t.destinationAddress == apInterface1.GetAddress (0))
        {
          double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
          std::cout << "Flow ID: " << i->first  << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput << " Mbps" << std::endl;
          totalThroughput1 = throughput;
        }
      else if (t.destinationAddress == apInterface2.GetAddress (0))
      {
        double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
        std::cout << "Flow ID: " << i->first  << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput << " Mbps" << std::endl;
        totalThroughput2 = throughput;
      }
    }

    std::cout << "Total Throughput BSS 1: " << totalThroughput1 << " Mbps" << std::endl;
    std::cout << "Total Throughput BSS 2: " << totalThroughput2 << " Mbps" << std::endl;

  Simulator::Destroy ();

  return 0;
}