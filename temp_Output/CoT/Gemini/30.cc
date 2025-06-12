#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTosExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nSta = 3;
  uint32_t mcs = 7;
  uint32_t channelWidth = 20;
  bool shortGuardInterval = true;
  double distance = 10.0;
  bool rtsCts = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("nSta", "Number of wifi stations", nSta);
  cmd.AddValue ("mcs", "HT MCS value (0 to 7)", mcs);
  cmd.AddValue ("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Use short guard interval if true", shortGuardInterval);
  cmd.AddValue ("distance", "Distance between AP and stations (meters)", distance);
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

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  std::string rate ("HtMcs" + std::to_string (mcs));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

  if (channelWidth == 40)
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue (rate),
                                    "ControlMode", StringValue ("VhtMcs0"),
                                    "ChannelWidth", UintegerValue (40),
                                    "ShortGuardIntervalSupported", BooleanValue (shortGuardInterval));
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue (rate),
                                    "ControlMode", StringValue ("VhtMcs0"));
    }

    if (rtsCts)
    {
      phy.Set ("RtsCtsThreshold", UintegerValue (0));
    }

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (nSta),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (staNodes);

  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.0]"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.0]"));
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staNodeInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apNodeInterfaces = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (apNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (apNodeInterfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.00002))); // send packets every 20 usec
  echoClient.SetAttribute ("PacketSize", UintegerValue (1472));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      clientApps.Add (echoClient.Install (staNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalRxBytes = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apNodeInterfaces.GetAddress (0))
        {
          totalRxBytes += i->second.rxBytes;
        }
    }

  double throughput = totalRxBytes * 8.0 / (10.0 - 2.0) / 1000000;
  std::cout << "Aggregated Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}