#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink.h"
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi11nTosThroughput");

struct SimulationParams
{
  uint32_t nStations;
  uint8_t mcs;
  uint32_t channelWidth;
  bool shortGuardInterval;
  double distance;
  bool useRtsCts;
  double simulationTime;
};

std::vector<uint32_t> tosValues = {0, 32, 96, 224};

void ThroughputMonitor (Ptr<PacketSink> sink, double startTime, double &aggThroughput, Time stopTime)
{
  static uint64_t lastTotalRx = 0;
  static double lastTime = startTime;
  double curTime = Simulator::Now().GetSeconds();
  uint64_t curTotalRx = sink->GetTotalRx ();
  if (curTime < stopTime.GetSeconds ())
    Simulator::Schedule (Seconds (0.1), &ThroughputMonitor, sink, startTime, std::ref(aggThroughput), stopTime);
  if (curTime > 0)
    aggThroughput += ((curTotalRx - lastTotalRx) * 8.0) / 1e6;
  lastTotalRx = curTotalRx;
  lastTime = curTime;
}

int main (int argc, char *argv[])
{
  SimulationParams params;
  params.nStations = 4;
  params.mcs = 7;
  params.channelWidth = 40;
  params.shortGuardInterval = true;
  params.distance = 10.0;
  params.useRtsCts = false;
  params.simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("nStations", "Number of Wi-Fi stations", params.nStations);
  cmd.AddValue ("mcs", "HT MCS value (0-7)", params.mcs);
  cmd.AddValue ("channelWidth", "Wi-Fi channel width (20 or 40 MHz)", params.channelWidth);
  cmd.AddValue ("shortGuardInterval", "Use short guard interval", params.shortGuardInterval);
  cmd.AddValue ("distance", "Distance between stations and AP (meters)", params.distance);
  cmd.AddValue ("useRtsCts", "Enable RTS/CTS", params.useRtsCts);
  cmd.AddValue ("simulationTime", "Simulation time (seconds)", params.simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (params.nStations);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelWidth", UintegerValue (params.channelWidth));
  phy.Set ("ShortGuardEnabled", BooleanValue (params.shortGuardInterval));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("HtMcs" + std::to_string(params.mcs)),
                               "ControlMode", StringValue ("HtMcs0"),
                               "RtsCtsThreshold", UintegerValue(params.useRtsCts ? 0 : 65535));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (params.distance),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (params.nStations),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator> ();
  apPos->Add (Vector (params.distance / 2.0, -params.distance, 0.0));
  mobility.SetPositionAllocator (apPos);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

  uint16_t port = 9;
  ApplicationContainer serverApps;
  Ptr<PacketSink> sink;

  // Each station sends UDP traffic to the AP, TOS values distributed round-robin
  for (uint32_t i = 0; i < params.nStations; ++i)
    {
      // OnOffApplication as UDP Client
      uint32_t tos = tosValues[i % tosValues.size()];
      OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress(0), port));
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
      onoff.SetAttribute ("PacketSize", UintegerValue (1472));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (params.simulationTime)));
      onoff.SetAttribute ("QosType", EnumValue (Ipv4Header::DscpType(tos))); // Not all traffic modules support this, but OnOff does for TOS
      Ptr<Application> app = onoff.Install (wifiStaNodes.Get(i)).Get(0);

      // Set TOS on socket by setting Type of Service value
      Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication>(app);
      if (onoffApp)
        {
          onoffApp->SetAttribute ("Ipv4Tos", UintegerValue(tos));
        }
    }

  // PacketSink on AP
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny (), port));
  serverApps = sinkHelper.Install (wifiApNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (params.simulationTime + 1));
  sink = DynamicCast<PacketSink> (serverApps.Get (0));

  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi11n_tos", apDevice, true);

  double aggregatedThroughput = 0.0;
  Simulator::Schedule (Seconds (1.0), &ThroughputMonitor,
                       sink, 1.0, std::ref(aggregatedThroughput),
                       Seconds (params.simulationTime));

  Simulator::Stop (Seconds (params.simulationTime + 1));
  Simulator::Run ();

  double mbits = (sink->GetTotalRx () * 8.0) / 1e6;
  double avgThroughput = mbits / params.simulationTime;

  std::cout << "==========Simulation Results==========" << std::endl;
  std::cout << "Number of Stations: " << params.nStations << std::endl;
  std::cout << "MCS: HtMcs" << unsigned(params.mcs) << std::endl;
  std::cout << "Channel Width: " << params.channelWidth << " MHz" << std::endl;
  std::cout << "Guard Interval: " << (params.shortGuardInterval ? "Short" : "Long") << std::endl;
  std::cout << "Distance: " << params.distance << " m" << std::endl;
  std::cout << "RTS/CTS: " << (params.useRtsCts ? "Enabled" : "Disabled") << std::endl;
  std::cout << "Simulation Time: " << params.simulationTime << " s" << std::endl;
  std::cout << "Total Received Bytes: " << sink->GetTotalRx () << std::endl;
  std::cout << "Aggregated UDP Throughput: " << avgThroughput << " Mbit/s" << std::endl;
  Simulator::Destroy ();
  return 0;
}