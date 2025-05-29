#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTosSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nSta = 5;
  double distance = 5.0;
  std::string mcs = "0";
  std::string channelWidth = "20";
  std::string guardInterval = "Long";
  bool rtsCts = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell applications to log if true", verbose);
  cmd.AddValue ("nSta", "Number of wifi STA devices", nSta);
  cmd.AddValue ("distance", "Distance between AP and STAs", distance);
  cmd.AddValue ("mcs", "HT MCS value (0-7)", mcs);
  cmd.AddValue ("channelWidth", "Channel width (20 or 40)", channelWidth);
  cmd.AddValue ("guardInterval", "Guard interval (Long or Short)", guardInterval);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (nSta);

  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (nSta),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (apNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (apInterfaces.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (MicroSeconds (1)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      clientApps.Add (client.Install (staNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", StringValue (channelWidth));

  if (guardInterval == "Short")
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue ("400ns"));
    }
  else
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue ("800ns"));
    }

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (guardInterval == "Short"));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxSupportedTxSpatialStreams", UintegerValue (1));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxSupportedRxSpatialStreams", UintegerValue (1));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/SupportedMcsSet", StringValue (mcs));

  if (rtsCts)
    {
        Config::SetDefault ("ns3::WifiMacQueue::RtsCtsThreshold", StringValue ("0"));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalBytes = 0.0;
  double totalTime = 0.0;

  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      if (t.destinationAddress == apInterfaces.GetAddress (0))
        {
          totalBytes += it->second.bytes;
          totalTime = it->second.timeLastRxPacket.Seconds () - it->second.timeFirstTxPacket.Seconds ();
          break; // Consider only the first flow to the AP
        }
    }

  Simulator::Destroy ();

  double throughput = (totalBytes * 8) / (totalTime * 1000000.0); // Mbit/s
  std::cout << "Aggregated UDP Throughput: " << throughput << " Mbit/s" << std::endl;

  return 0;
}