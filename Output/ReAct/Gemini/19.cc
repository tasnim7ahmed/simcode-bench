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
#include <sstream>

using namespace ns3;

// Function to convert Wi-Fi version string to standard and band
std::pair<WifiStandard, WifiPhyBand>
GetWifiStandardAndBand (const std::string& wifiVersion)
{
  if (wifiVersion == "80211a")
    {
      return std::make_pair (WIFI_PHY_STANDARD_80211a, WIFI_PHY_BAND_5GHZ);
    }
  else if (wifiVersion == "80211b")
    {
      return std::make_pair (WIFI_PHY_STANDARD_80211b, WIFI_PHY_BAND_2GHZ);
    }
  else if (wifiVersion == "80211g")
    {
      return std::make_pair (WIFI_PHY_STANDARD_80211g, WIFI_PHY_BAND_2GHZ);
    }
  else if (wifiVersion == "80211n")
    {
      return std::make_pair (WIFI_PHY_STANDARD_80211n, WIFI_PHY_BAND_2GHZ);
    }
  else if (wifiVersion == "80211ac")
    {
      return std::make_pair (WIFI_PHY_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ);
    }
  else if (wifiVersion == "80211ax")
    {
      return std::make_pair (WIFI_PHY_STANDARD_80211ax, WIFI_PHY_BAND_5GHZ);
    }
  else
    {
      return std::make_pair (WIFI_PHY_STANDARD_UNSPECIFIED, WIFI_PHY_BAND_UNSPECIFIED);
    }
}


int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string staWifiStandard = "80211b";
  std::string apWifiStandard = "80211ac";
  std::string rateAdaptationAlgorithm = "AarfWifiManager";
  bool apSendsTraffic = false;
  bool staSendsTraffic = true;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("staWifiStandard", "Wi-Fi standard for STA (e.g., 80211b)", staWifiStandard);
  cmd.AddValue ("apWifiStandard", "Wi-Fi standard for AP (e.g., 80211ac)", apWifiStandard);
  cmd.AddValue ("rateAdaptationAlgorithm", "Rate adaptation algorithm", rateAdaptationAlgorithm);
  cmd.AddValue ("apSendsTraffic", "AP sends traffic to STA", apSendsTraffic);
  cmd.AddValue ("staSendsTraffic", "STA sends traffic to AP", staSendsTraffic);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  // Convert Wi-Fi standard strings to enums
  auto staStandardPair = GetWifiStandardAndBand (staWifiStandard);
  WifiStandard staWifiPhyStandard = staStandardPair.first;
  WifiPhyBand staWifiPhyBand = staStandardPair.second;

  auto apStandardPair = GetWifiStandardAndBand (apWifiStandard);
  WifiStandard apWifiPhyStandard = apStandardPair.first;
  WifiPhyBand apWifiPhyBand = apStandardPair.second;
  
  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  // Set up channel and PHY
  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
  phyHelper.SetChannel (channelHelper.Create ());

  // Set up MAC and SSID
  WifiHelper wifiHelper;
  wifiHelper.SetRemoteStationManager (rateAdaptationAlgorithm);

  WifiMacHelper macHelper;
  Ssid ssid = Ssid ("ns3-wifi");
  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifiHelper.Install (phyHelper, macHelper, staNode);

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices = wifiHelper.Install (phyHelper, macHelper, apNode);

  // Set Wi-Fi standard for the STA and AP
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Standard", EnumValue (staWifiPhyStandard));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/Standard", EnumValue (apWifiPhyStandard));
  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (80));

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
  mobility.Install (apNode);

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-10, 10, -10, 10)));
  mobility.Install (staNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  // UDP Echo server on AP
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  // UDP Echo client on STA
  UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  if (staSendsTraffic) {
        clientApps = echoClient.Install (staNode.Get (0));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (simulationTime));
  }

  // UDP Echo server on STA
  UdpEchoServerHelper echoServerSta (8);
  ApplicationContainer serverAppsSta = echoServerSta.Install (staNode.Get (0));
  serverAppsSta.Start (Seconds (1.0));
  serverAppsSta.Stop (Seconds (simulationTime));

  // UDP Echo client on AP
  UdpEchoClientHelper echoClientAp (staInterfaces.GetAddress (0), 8);
  echoClientAp.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  echoClientAp.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClientAp.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientAppsAp;
  if (apSendsTraffic) {
      clientAppsAp = echoClientAp.Install (apNode.Get (0));
      clientAppsAp.Start (Seconds (2.0));
      clientAppsAp.Stop (Seconds (simulationTime));
  }
  // Global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // PCAP Tracing
  if (enablePcap)
    {
      phyHelper.EnablePcap ("wifi-backward-compatibility", apDevices);
      phyHelper.EnablePcap ("wifi-backward-compatibility", staDevices);
    }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print flow stats
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}