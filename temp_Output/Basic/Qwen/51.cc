#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Tcp80211nPerformance");

int main (int argc, char *argv[])
{
  std::string tcpCongestionControl = "TcpNewReno";
  DataRate dataRate ("10Mbps");
  uint32_t payloadSize = 1448;
  Time interval = MilliSeconds (100);
  double simulationTime = 10.0;
  bool pcapTracing = false;
  uint32_t maxmpdu = 4095;
  uint32_t maxamsdu = 65535;
  uint32_t queueSize = 1000;
  DataRate phyRate ("650Mbps");

  CommandLine cmd (__FILE__);
  cmd.AddValue ("tcpCongestionControl", "TCP congestion control algorithm", tcpCongestionControl);
  cmd.AddValue ("dataRate", "Application data rate", dataRate);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcapTracing", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue ("phyRate", "PHY layer bitrate", phyRate);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                      TypeIdValue (TypeId::LookupByName (tcpCongestionControl)));

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  phy.Set ("RemoteStationManagerDataMode", StringValue (phyRate.GetModeString ()));
  phy.Set ("RemoteStationManagerCtrlMode", StringValue (phyRate.GetModeString ()));

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyRate.GetModeString ()), "ControlMode", StringValue (phyRate.GetModeString ()));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (Ssid ("network-80211n")),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (Ssid ("network-80211n")),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (2.5)),
               "MaxAmsduSize", UintegerValue (maxamsdu),
               "MaxMpduSize", UintegerValue (maxmpdu));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign (staDevice);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (apInterface.GetAddress (0), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (wifiApNode.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 0.1));

  OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", DataRateValue (dataRate));
  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer srcApp = onoff.Install (wifiStaNode.Get (0));
  srcApp.Start (Seconds (1.0));
  srcApp.Stop (Seconds (simulationTime + 0.1));

  if (pcapTracing)
    {
      phy.EnablePcap ("tcp-80211n-performance", apDevice.Get (0));
    }

  Simulator::Stop (Seconds (simulationTime + 0.5));
  Simulator::Run ();

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  double totalRx = sink->GetTotalRx () * 8.0 / 1e6;
  NS_LOG_UNCOND ("Total throughput: " << totalRx / simulationTime << " Mbps");

  Simulator::Destroy ();

  return 0;
}