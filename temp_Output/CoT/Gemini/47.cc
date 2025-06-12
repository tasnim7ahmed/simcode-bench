#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpatialReuseSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool enableObssPd = true;
  double simulationTime = 10;
  std::string dataRate = "54Mbps";
  int packetSize = 1472;
  double sta1Ap1Distance = 10;
  double sta2Ap2Distance = 10;
  double ap1Ap2Distance = 50;
  double transmitPowerDbm = 15;
  double ccaEdThresholdDbm = -82;
  double obssPdThresholdDbm = -72;
  std::string channelWidth = "20MHz";
  double sta1Start = 0.1;
  double sta2Start = 0.1;
  double sta1Interval = 0.001;
  double sta2Interval = 0.001;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue ("enableObssPd", "Enable OBSS PD spatial reuse.", enableObssPd);
  cmd.AddValue ("simulationTime", "Simulation duration in seconds.", simulationTime);
  cmd.AddValue ("dataRate", "Data rate for transmissions.", dataRate);
  cmd.AddValue ("packetSize", "Size of packets in bytes.", packetSize);
  cmd.AddValue ("sta1Ap1Distance", "Distance between STA1 and AP1 in meters.", sta1Ap1Distance);
  cmd.AddValue ("sta2Ap2Distance", "Distance between STA2 and AP2 in meters.", sta2Ap2Distance);
  cmd.AddValue ("ap1Ap2Distance", "Distance between AP1 and AP2 in meters.", ap1Ap2Distance);
  cmd.AddValue ("transmitPowerDbm", "Transmit power in dBm.", transmitPowerDbm);
  cmd.AddValue ("ccaEdThresholdDbm", "CCA ED threshold in dBm.", ccaEdThresholdDbm);
  cmd.AddValue ("obssPdThresholdDbm", "OBSS PD threshold in dBm.", obssPdThresholdDbm);
  cmd.AddValue ("channelWidth", "Channel width (20MHz, 40MHz, 80MHz).", channelWidth);
  cmd.AddValue ("sta1Start", "Start time for STA1 transmission.", sta1Start);
  cmd.AddValue ("sta2Start", "Start time for STA2 transmission.", sta2Start);
  cmd.AddValue ("sta1Interval", "Packet interval for STA1 traffic.", sta1Interval);
  cmd.AddValue ("sta2Interval", "Packet interval for STA2 traffic.", sta2Interval);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("SpatialReuseSimulation", LOG_LEVEL_INFO);
    }

  NodeContainer apNodes;
  apNodes.Create (2);
  NodeContainer staNodes;
  staNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();

  if (channelWidth == "20MHz")
    {
      phy.Set ("ChannelWidth", UintegerValue (20));
    }
  else if (channelWidth == "40MHz")
    {
      phy.Set ("ChannelWidth", UintegerValue (40));
    }
  else if (channelWidth == "80MHz")
    {
      phy.Set ("ChannelWidth", UintegerValue (80));
    }
  else
    {
      std::cerr << "Invalid channel width.  Using 20MHz.\n";
      phy.Set ("ChannelWidth", UintegerValue (20));
    }

  phy.Set ("TxPowerStart", DoubleValue (transmitPowerDbm));
  phy.Set ("TxPowerEnd", DoubleValue (transmitPowerDbm));
  phy.Set ("CcaEdThreshold", DoubleValue (ccaEdThresholdDbm));
  phy.Set ("ObssPdThreshold", DoubleValue (obssPdThresholdDbm));

  if (!enableObssPd)
    {
      phy.Set ("ObssPdThreshold", DoubleValue (999));
    }

  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  channel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ax);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns3-80211ax-1");
  Ssid ssid2 = Ssid ("ns3-80211ax-2");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));
  NetDeviceContainer apDevs1 = wifi.Install (phy, mac, apNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));
  NetDeviceContainer apDevs2 = wifi.Install (phy, mac, apNodes.Get (1));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevs1 = wifi.Install (phy, mac, staNodes.Get (0));

    mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevs2 = wifi.Install (phy, mac, staNodes.Get (1));

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
  mobility.Install (staNodes);

  Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  Ptr<ConstantPositionMobilityModel> ap1Mobility = apNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  Ptr<ConstantPositionMobilityModel> sta2Mobility = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  Ptr<ConstantPositionMobilityModel> ap2Mobility = apNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();

  sta1Mobility->SetPosition(Vector(0.0, 0.0, 0.0));
  ap1Mobility->SetPosition(Vector(sta1Ap1Distance, 0.0, 0.0));
  sta2Mobility->SetPosition(Vector(ap1Ap2Distance, 0.0, 0.0));
  ap2Mobility->SetPosition(Vector(ap1Ap2Distance + sta2Ap2Distance, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevs1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign (staDevs1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevs2);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign (staDevs2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpClientHelper sta1Client (apInterfaces1.GetAddress (0), port);
  sta1Client.SetAttribute ("Interval", TimeValue (Seconds (sta1Interval)));
  sta1Client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer sta1App = sta1Client.Install (staNodes.Get (0));
  sta1App.Start (Seconds (sta1Start));
  sta1App.Stop (Seconds (simulationTime));

  UdpClientHelper sta2Client (apInterfaces2.GetAddress (0), port);
  sta2Client.SetAttribute ("Interval", TimeValue (Seconds (sta2Interval)));
  sta2Client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer sta2App = sta2Client.Install (staNodes.Get (1));
  sta2App.Start (Seconds (sta2Start));
  sta2App.Stop (Seconds (simulationTime));

  UdpServerHelper ap1Server (port);
  ApplicationContainer ap1App = ap1Server.Install (apNodes.Get (0));
  ap1App.Start (Seconds (0.0));
  ap1App.Stop (Seconds (simulationTime + 1));

  UdpServerHelper ap2Server (port);
  ApplicationContainer ap2App = ap2Server.Install (apNodes.Get (1));
  ap2App.Start (Seconds (0.0));
  ap2App.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  std::ofstream obssPdResetStream;
  obssPdResetStream.open ("obss_pd_resets.txt");

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ObssPdReset", MakeBoundCallback (&obssPdResetStream, &std::ofstream::put, &std::ofstream::is_open));

  Simulator::Run ();

  obssPdResetStream.close ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double throughput1 = 0.0;
  double throughput2 = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apInterfaces1.GetAddress (0))
        {
          throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        }
      else if (t.destinationAddress == apInterfaces2.GetAddress (0))
        {
          throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        }
    }

  std::cout << "BSS1 Throughput: " << throughput1 << " Mbps" << std::endl;
  std::cout << "BSS2 Throughput: " << throughput2 << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}