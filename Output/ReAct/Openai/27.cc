#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy-standard.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiNGoodputTest");

void
CalculateGoodput (Ptr<PacketSink> sink, double startTime, double expectedThroughput)
{
  double goodput = (sink->GetTotalRx () * 8.0) / (Simulator::Now ().GetSeconds () - startTime) / 1e6; // Mbps
  std::cout << Simulator::Now ().GetSeconds () << "s: Goodput = " << goodput << " Mbps";
  if (expectedThroughput > 0.0)
    {
      std::cout << " | Expected >= " << expectedThroughput << " Mbps";
    }
  std::cout << std::endl;
  Simulator::Schedule (Seconds (1.0), &CalculateGoodput, sink, startTime, expectedThroughput);
}

int
main (int argc, char *argv[])
{
  uint32_t mcs = 7;
  uint32_t channelWidth = 40;
  bool shortGuardInterval = true;
  double distance = 5.0;
  bool useRtsCts = false;
  double simulationTime = 10.0;
  bool useTcp = false;
  double expectedThroughput = 0.0;

  CommandLine cmd;
  cmd.AddValue ("mcs", "MCS value (0-7)", mcs);
  cmd.AddValue ("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Use Short Guard Interval (true/false)", shortGuardInterval);
  cmd.AddValue ("distance", "Distance between STA and AP (meters)", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS (true/false)", useRtsCts);
  cmd.AddValue ("simulationTime", "Simulation time (seconds)", simulationTime);
  cmd.AddValue ("tcp", "Use TCP instead of UDP", useTcp);
  cmd.AddValue ("expectedThroughput", "Expected throughput in Mbps (printed)", expectedThroughput);
  cmd.Parse (argc, argv);

  if (mcs > 7) mcs = 7;
  if (channelWidth != 20 && channelWidth != 40) channelWidth = 20;

  // Nodes creation: 1 AP, 1 STA
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  // WiFi PHY and channel config
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelWidth", UintegerValue (channelWidth));
  phy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
  phy.Set ("Frequency", UintegerValue (5180)); // 5.18 GHz
  phy.Set ("Standard", StringValue ("802.11n-5GHz"));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

  // MCS, HT, DataMode
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n-test");
  HtWifiMacHelper htMac;
  WifiRemoteStationManagerHelper wifiManager;
  wifiManager.SetType ("ns3::IdealWifiManager",
                       "RtsCtsThreshold", UintegerValue (useRtsCts ? 0 : 999999),
                       "MaxSsrc", UintegerValue (7),
                       "MaxSlrc", UintegerValue (7),
                       "DataMode", StringValue ("HtMcs" + std::to_string (mcs)),
                       "ControlMode", StringValue ("HtMcs0"));

  NetDeviceContainer staDevice, apDevice;
  htMac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
  staDevice = wifi.Install (phy, htMac, wifiStaNode);

  htMac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
  apDevice = wifi.Install (phy, htMac, wifiApNode);

  for (auto dev : {staDevice.Get (0), apDevice.Get (0)})
    {
      dev->GetObject<WifiNetDevice> ()->GetWifiPhy ()->SetShortGuardEnabled (shortGuardInterval);
      dev->GetObject<WifiNetDevice> ()->GetWifiPhy ()->SetChannelWidth (channelWidth);
    }
  wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                               "RtsCtsThreshold", UintegerValue (useRtsCts ? 0 : 999999),
                               "MaxSsrc", UintegerValue (7),
                               "MaxSlrc", UintegerValue (7),
                               "DataMode", StringValue ("HtMcs" + std::to_string (mcs)),
                               "ControlMode", StringValue ("HtMcs0"));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 1.0));
  positionAlloc->Add (Vector (distance, 0.0, 1.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface, apInterface;
  apInterface = address.Assign (apDevice);
  staInterface = address.Assign (staDevice);

  // Application setup
  uint16_t port = 9;
  ApplicationContainer sinkApp;
  Ptr<PacketSink> sink;
  if (useTcp)
    {
      // TCP sink at STA
      Address sinkAddress (InetSocketAddress (staInterface.GetAddress (0), port));
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
      sinkApp = packetSinkHelper.Install (wifiStaNode.Get (0));
      sink = StaticCast<PacketSink> (sinkApp.Get (0));

      // BulkSend from AP
      BulkSendHelper bulk ("ns3::TcpSocketFactory", sinkAddress);
      bulk.SetAttribute ("MaxBytes", UintegerValue (0));
      ApplicationContainer sourceApp = bulk.Install (wifiApNode.Get (0));
      sourceApp.Start (Seconds (1.0));
      sourceApp.Stop (Seconds (simulationTime));
    }
  else
    {
      // UDP sink at STA
      Address sinkAddress (InetSocketAddress (staInterface.GetAddress (0), port));
      PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
      sinkApp = packetSinkHelper.Install (wifiStaNode.Get (0));
      sink = StaticCast<PacketSink> (sinkApp.Get (0));

      // OnOff from AP
      OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("600Mbps")));
      onoff.SetAttribute ("PacketSize", UintegerValue (1472));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
      ApplicationContainer sourceApp = onoff.Install (wifiApNode.Get (0));
    }
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  Simulator::Schedule (Seconds (1.1), &CalculateGoodput, sink, 1.1, expectedThroughput);
  Simulator::Stop (Seconds (simulationTime + 0.5));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}