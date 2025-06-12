#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

struct WifiStandardPair
{
  WifiStandard standard;
  WifiPhyBand band;
};

WifiStandardPair
ParseWifiStandard (const std::string &version)
{
  if (version == "80211a")
    return {WIFI_STANDARD_80211a, WIFI_PHY_BAND_5GHZ};
  else if (version == "80211b")
    return {WIFI_STANDARD_80211b, WIFI_PHY_BAND_2_4GHZ};
  else if (version == "80211g")
    return {WIFI_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ};
  else if (version == "80211n_2_4GHz")
    return {WIFI_STANDARD_80211n, WIFI_PHY_BAND_2_4GHZ};
  else if (version == "80211n_5GHz")
    return {WIFI_STANDARD_80211n, WIFI_PHY_BAND_5GHZ};
  else if (version == "80211ac")
    return {WIFI_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ};
  else if (version == "80211ax_2_4GHz")
    return {WIFI_STANDARD_80211ax, WIFI_PHY_BAND_2_4GHZ};
  else if (version == "80211ax_5GHz")
    return {WIFI_STANDARD_80211ax, WIFI_PHY_BAND_5GHZ};
  return {WIFI_STANDARD_80211a, WIFI_PHY_BAND_5GHZ};
}

double
CalculateThroughput (Ptr<PacketSink> sink, double startTime, double stopTime)
{
  uint64_t totalBytes = sink->GetTotalRx ();
  double throughput = totalBytes * 8.0 / (stopTime - startTime) / 1e6; // Mbps
  return throughput;
}

int main (int argc, char *argv[])
{
  std::string apWifiStandardStr = "80211ac";
  std::string staWifiStandardStr = "80211a";
  std::string apRate = "ns3::AarfWifiManager";
  std::string staRate = "ns3::MinstrelWifiManager";
  std::string trafficDirection = "STAtoAP"; // Options: "APtoSTA", "STAtoAP", "BiDir"
  double simulationTime = 10.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("apStandard", "Wi-Fi standard for AP ('80211a', '80211ac', etc.)", apWifiStandardStr);
  cmd.AddValue ("staStandard", "Wi-Fi standard for STA ('80211a', '80211ac', etc.)", staWifiStandardStr);
  cmd.AddValue ("apRate", "Rate adaptation for AP", apRate);
  cmd.AddValue ("staRate", "Rate adaptation for STA", staRate);
  cmd.AddValue ("trafficDirection", "Traffic direction", trafficDirection);
  cmd.Parse (argc, argv);

  WifiStandardPair apWifi = ParseWifiStandard (apWifiStandardStr);
  WifiStandardPair staWifi = ParseWifiStandard (staWifiStandardStr);

  NodeContainer wifiApNode;
  wifiApNode.Create (1);
  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);

  // Wi-Fi Channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelNumber", UintegerValue (apWifi.band == WIFI_PHY_BAND_2_4GHZ ? 6 : 36));

  // Wi-Fi Helpers for AP and STA
  WifiHelper wifiApHelper;
  wifiApHelper.SetStandard (apWifi.standard);
  wifiApHelper.SetRemoteStationManager (apRate);

  WifiHelper wifiStaHelper;
  wifiStaHelper.SetStandard (staWifi.standard);
  wifiStaHelper.SetRemoteStationManager (staRate);

  // Phy Helpers for AP and STA (allow different bands)
  YansWifiPhyHelper phyAp = phy;
  YansWifiPhyHelper phySta = phy;

  if (apWifi.band != staWifi.band)
    {
      if (apWifi.band == WIFI_PHY_BAND_2_4GHZ)
        phyAp.Set ("ChannelNumber", UintegerValue (6));
      else
        phyAp.Set ("ChannelNumber", UintegerValue (36));
      if (staWifi.band == WIFI_PHY_BAND_2_4GHZ)
        phySta.Set ("ChannelNumber", UintegerValue (6));
      else
        phySta.Set ("ChannelNumber", UintegerValue (36));
    }

  Ssid ssid = Ssid ("wifi-compat");

  WifiMacHelper macAp;
  macAp.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));

  WifiMacHelper macSta;
  macSta.SetType ("ns3::StaWifiMac",
                  "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice, staDevice;
  apDevice = wifiApHelper.Install (phyAp, macAp, wifiApNode);
  staDevice = wifiStaHelper.Install (phySta, macSta, wifiStaNode);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
  positionAlloc->Add (Vector (5.0, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-20, 20, -20, 20)),
                             "Distance", DoubleValue (2.5),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install (wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface, staInterface;
  apInterface = address.Assign (apDevice);
  staInterface = address.Assign (staDevice);

  // UDP Applications
  uint16_t port = 4000;
  ApplicationContainer apApp, staApp, sinkApp;
  Ptr<PacketSink> apSink, staSink;

  if (trafficDirection == "STAtoAP" || trafficDirection == "BiDir")
    {
      // AP as sink, STA as client
      PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
      sinkApp = sink.Install (wifiApNode.Get (0));
      apSink = StaticCast<PacketSink> (sinkApp.Get (0));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simulationTime+1.0));

      UdpClientHelper client (apInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0005)));
      client.SetAttribute ("PacketSize", UintegerValue (1400));
      staApp = client.Install (wifiStaNode.Get (0));
      staApp.Start (Seconds (1.0));
      staApp.Stop (Seconds (simulationTime));
    }
  if (trafficDirection == "APtoSTA" || trafficDirection == "BiDir")
    {
      // STA as sink, AP as client
      PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (staInterface.GetAddress (0), port));
      ApplicationContainer staSinkApp = sink.Install (wifiStaNode.Get (0));
      staSink = StaticCast<PacketSink> (staSinkApp.Get (0));
      staSinkApp.Start (Seconds (0.0));
      staSinkApp.Stop (Seconds (simulationTime+1.0));

      UdpClientHelper client (staInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0005)));
      client.SetAttribute ("PacketSize", UintegerValue (1400));
      apApp = client.Install (wifiApNode.Get (0));
      apApp.Start (Seconds (1.0));
      apApp.Stop (Seconds (simulationTime));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  phyAp.EnablePcap ("backward-compatible-ap", apDevice.Get (0), true);
  phySta.EnablePcap ("backward-compatible-sta", staDevice.Get (0), true);

  Simulator::Stop (Seconds (simulationTime+2.0));

  Simulator::Run ();

  if (trafficDirection == "STAtoAP" || trafficDirection == "BiDir")
    {
      double throughput = CalculateThroughput (apSink, 1.0, simulationTime);
      std::cout << "Throughput from STA to AP: " << throughput << " Mbps" << std::endl;
    }
  if (trafficDirection == "APtoSTA" || trafficDirection == "BiDir")
    {
      double throughput = CalculateThroughput (staSink, 1.0, simulationTime);
      std::cout << "Throughput from AP to STA: " << throughput << " Mbps" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}