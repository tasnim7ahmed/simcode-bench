#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpatialReuse80211ax");

int main (int argc, char *argv[])
{
  std::string phyMode ("HeMcs0");
  double distanceApSta1 = 5.0;   // d1
  double distanceApSta2 = 5.0;   // d2
  double distanceApAp = 20.0;    // d3
  double txPowerAp = 20;         // dBm
  double txPowerSta = 15;        // dBm
  double ccaEdThreshold = -79;   // dBm
  double obssPdThreshold = -82;  // dBm
  bool enableObssPd = true;
  uint32_t channelWidth = 20;    // MHz
  uint32_t packetSize = 1024;
  Time simulationTime = Seconds(10);
  std::string dataRateSta1 = "100Mbps";
  std::string dataRateSta2 = "100Mbps";
  std::string logFile = "spatial_reuse_log.txt";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("distanceApSta1", "Distance between AP1 and STA1 in meters", distanceApSta1);
  cmd.AddValue ("distanceApSta2", "Distance between AP2 and STA2 in meters", distanceApSta2);
  cmd.AddValue ("distanceApAp", "Distance between AP1 and AP2 in meters", distanceApAp);
  cmd.AddValue ("txPowerAp", "Transmit power of Access Points in dBm", txPowerAp);
  cmd.AddValue ("txPowerSta", "Transmit power of Stations in dBm", txPowerSta);
  cmd.AddValue ("ccaEdThreshold", "CCA-ED threshold in dBm", ccaEdThreshold);
  cmd.AddValue ("obssPdThreshold", "OBSS-PD threshold in dBm", obssPdThreshold);
  cmd.AddValue ("enableObssPd", "Enable or disable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue ("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
  cmd.AddValue ("packetSize", "Size of application packets in bytes", packetSize);
  cmd.AddValue ("simulationTime", "Total simulation time", simulationTime);
  cmd.AddValue ("dataRateSta1", "Data rate for STA1 traffic", dataRateSta1);
  cmd.AddValue ("dataRateSta2", "Data rate for STA2 traffic", dataRateSta2);
  cmd.AddValue ("logFile", "Output file to log spatial reuse reset events", logFile);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiPhy::ChannelWidth", UintegerValue (channelWidth));
  Config::SetDefault ("ns3::WifiPhy::TxPowerStart", DoubleValue (txPowerSta));
  Config::SetDefault ("ns3::WifiPhy::TxPowerEnd", DoubleValue (txPowerSta));
  Config::SetDefault ("ns3::WifiPhy::EnergyDetectionThreshold", DoubleValue (ccaEdThreshold));
  Config::SetDefault ("ns3::HeConfiguration::EnableObssPdOverride", BooleanValue (true));
  Config::SetDefault ("ns3::HeConfiguration::ObssPdLevel", DoubleValue (obssPdThreshold));

  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (2);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
  phyHelper.SetChannel (channelHelper.Create ());

  WifiMacHelper macHelper;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_STANDARD_80211ax_5GHZ);
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode", StringValue (phyMode));

  Ssid ssid1 = Ssid ("BSS1");
  Ssid ssid2 = Ssid ("BSS2");

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid1),
                     "BeaconInterval", TimeValue (MicroSeconds (102400)));
  NetDeviceContainer apDevices1 = wifiHelper.Install (phyHelper, macHelper, apNodes.Get (0));

  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid1),
                     "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices1 = wifiHelper.Install (phyHelper, macHelper, staNodes.Get (0));

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid2),
                     "BeaconInterval", TimeValue (MicroSeconds (102400)));
  NetDeviceContainer apDevices2 = wifiHelper.Install (phyHelper, macHelper, apNodes.Get (1));

  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid2),
                     "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices2 = wifiHelper.Install (phyHelper, macHelper, staNodes.Get (1));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  positionAlloc->Add (Vector (0.0, 0.0, 0.0));     // AP1
  positionAlloc->Add (Vector (distanceApAp, 0.0, 0.0)); // AP2
  positionAlloc->Add (Vector (-distanceApSta1, 0.0, 0.0)); // STA1
  positionAlloc->Add (Vector (distanceApAp + distanceApSta2, 0.0, 0.0)); // STA2

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);
  mobility.Install (staNodes);

  phyHelper.Set ("TxPowerStart", DoubleValue (txPowerAp));
  phyHelper.Set ("TxPowerEnd", DoubleValue (txPowerAp));

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevices1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign (staDevices1);

  address.NewNetwork ();
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevices2);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign (staDevices2);

  UdpServerHelper server1 (9);
  ApplicationContainer serverApp1 = server1.Install (apNodes.Get (0));
  serverApp1.Start (Seconds (0.0));
  serverApp1.Stop (simulationTime);

  UdpClientHelper client1 (apInterfaces1.GetAddress (0), 9);
  client1.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  client1.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp1 = client1.Install (staNodes.Get (0));
  clientApp1.Start (Seconds (0.5));
  clientApp1.Stop (simulationTime);

  UdpServerHelper server2 (9);
  ApplicationContainer serverApp2 = server2.Install (apNodes.Get (1));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (simulationTime);

  UdpClientHelper client2 (apInterfaces2.GetAddress (0), 9);
  client2.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  client2.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  client2.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp2 = client2.Install (staNodes.Get (1));
  clientApp2.Start (Seconds (0.5));
  clientApp2.Stop (simulationTime);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (simulationTime);
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  std::ofstream logOut (logFile.c_str ());
  if (!logOut.fail ())
    {
      logOut << "Spatial Reuse Configuration:" << std::endl;
      logOut << "OBSS-PD Enabled: " << (enableObssPd ? "Yes" : "No") << std::endl;
      logOut << "OBSS-PD Threshold: " << obssPdThreshold << " dBm" << std::endl;
      logOut << "Channel Width: " << channelWidth << " MHz" << std::endl;
    }

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalThroughput1 = 0;
  double totalThroughput2 = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.sourceAddress == staInterfaces1.GetAddress (0))
        {
          totalThroughput1 += i->second.rxBytes * 8.0 / simulationTime.GetSeconds () / 1000 / 1000;
        }
      else if (t.sourceAddress == staInterfaces2.GetAddress (0))
        {
          totalThroughput2 += i->second.rxBytes * 8.0 / simulationTime.GetSeconds () / 1000 / 1000;
        }
    }

  std::cout << "Throughput BSS1: " << totalThroughput1 << " Mbps" << std::endl;
  std::cout << "Throughput BSS2: " << totalThroughput2 << " Mbps" << std::endl;

  if (!logOut.fail ())
    {
      logOut << "Throughput BSS1: " << totalThroughput1 << " Mbps" << std::endl;
      logOut << "Throughput BSS2: " << totalThroughput2 << " Mbps" << std::endl;
      logOut.close ();
    }

  Simulator::Destroy ();
  return 0;
}