#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-phy-standard.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAxSpatialReuse");

void RxEventLogger (Ptr<const Packet> packet, const Address &from, const Address &to, uint32_t channelFreqMhz,
                    WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t channelWidth, bool isObssPd, std::string logFile)
{
  std::ofstream outFile;
  outFile.open (logFile, std::ios::app);
  if (outFile.is_open ())
    {
      outFile << Simulator::Now ().GetSeconds () << " "
              << "OBSS_PD=" << isObssPd << " "
              << "RxPower=" << signalNoise.signal << "dBm "
              << "ChannelWidth=" << channelWidth << " "
              << "\n";
      outFile.close ();
    }
}

int main (int argc, char *argv[])
{
  double d1 = 5.0;
  double d2 = 5.0;
  double d3 = 30.0;
  double txPowerDbm = 21.0;
  double ccaEdThresholdDbm = -62.0;
  double obssPdThresholdDbm = -72.0;
  bool enableObssPd = true;
  double simulationTime = 10.0;
  std::string trafficInterval = "1ms";
  uint32_t packetSize = 1472;
  uint32_t channelWidth = 20;
  std::string phyMode = "HeMcs9";
  std::string logFile1 = "bss1_obsspd.log";
  std::string logFile2 = "bss2_obsspd.log";
  CommandLine cmd;
  cmd.AddValue ("d1", "Distance STA1-AP1 (m)", d1);
  cmd.AddValue ("d2", "Distance STA2-AP2 (m)", d2);
  cmd.AddValue ("d3", "Distance AP1-AP2 (m)", d3);
  cmd.AddValue ("txPowerDbm", "Transmit Power in dBm", txPowerDbm);
  cmd.AddValue ("ccaEdThresholdDbm", "CCA-ED Threshold (dBm)", ccaEdThresholdDbm);
  cmd.AddValue ("obssPdThresholdDbm", "OBSS-PD Threshold (dBm)", obssPdThresholdDbm);
  cmd.AddValue ("enableObssPd", "Enable OBSS-PD", enableObssPd);
  cmd.AddValue ("simulationTime", "Total duration (s)", simulationTime);
  cmd.AddValue ("trafficInterval", "Interval for each packet", trafficInterval);
  cmd.AddValue ("packetSize", "Packet size, bytes", packetSize);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("phyMode", "PHY mode/modulation (e.g., HeMcs9)", phyMode);
  cmd.Parse (argc, argv);

  NodeContainer apNodes, staNodes;
  apNodes.Create (2);
  staNodes.Create (2);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel (channel);
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7.0));
  wifiPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
  wifiPhy.Set ("Frequency", UintegerValue (5180));
  wifiPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdThresholdDbm));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_5GHZ);
  wifi.SetRemoteStationManager ("ns3::HeWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue (phyMode));

  WifiMacHelper wifiMac;
  Ssid ssid1 = Ssid ("bss-1");
  Ssid ssid2 = Ssid ("bss-2");

  // AP1 and STA1
  NetDeviceContainer ap1Devices, sta1Devices;
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid1));
  ap1Devices = wifi.Install (wifiPhy, wifiMac, apNodes.Get (0));
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid1),
                   "ActiveProbing", BooleanValue (false));
  sta1Devices = wifi.Install (wifiPhy, wifiMac, staNodes.Get (0));

  // AP2 and STA2
  NetDeviceContainer ap2Devices, sta2Devices;
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid2));
  ap2Devices = wifi.Install (wifiPhy, wifiMac, apNodes.Get (1));
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid2),
                   "ActiveProbing", BooleanValue (false));
  sta2Devices = wifi.Install (wifiPhy, wifiMac, staNodes.Get (1));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  // Place AP1 at (0, 0, 1.2)
  positionAlloc->Add (Vector (0.0, 0.0, 1.2));
  // Place STA1 at (d1, 0, 1.0)
  positionAlloc->Add (Vector (d1, 0.0, 1.0));
  // Place AP2 at (d3, 0, 1.2)
  positionAlloc->Add (Vector (d3, 0.0, 1.2));
  // Place STA2 at (d3 + d2, 0, 1.0)
  positionAlloc->Add (Vector (d3 + d2, 0.0, 1.0));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (positionAlloc);

  NodeContainer allNodes;
  allNodes.Add (apNodes);
  allNodes.Add (staNodes);
  mobility.Install (allNodes);

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sta1If = address.Assign (sta1Devices);
  address.NewNetwork ();
  Ipv4InterfaceContainer ap1If = address.Assign (ap1Devices);
  address.NewNetwork ();
  Ipv4InterfaceContainer sta2If = address.Assign (sta2Devices);
  address.NewNetwork ();
  Ipv4InterfaceContainer ap2If = address.Assign (ap2Devices);

  // OBSS-PD configuration
  if (enableObssPd)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/SpatialReuse/EnableObssPd", BooleanValue (true));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/SpatialReuse/ObssPdLevel", DoubleValue (obssPdThresholdDbm));
    }
  else
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/SpatialReuse/EnableObssPd", BooleanValue (false));
    }

  // Logging spatial reuse reset events for BSS1 and BSS2
  Config::Connect ("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
                   MakeBoundCallback (&RxEventLogger, logFile1));
  Config::Connect ("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
                   MakeBoundCallback (&RxEventLogger, logFile2));

  // UDP Traffic: STA1 -> AP1, STA2 -> AP2
  uint16_t port1 = 50001;
  uint16_t port2 = 50002;

  // OnOff application to generate traffic: always On
  OnOffHelper onoff1 ("ns3::UdpSocketFactory", InetSocketAddress (ap1If.GetAddress (0), port1));
  onoff1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff1.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  onoff1.SetAttribute ("Interval", StringValue (trafficInterval));
  ApplicationContainer sta1App = onoff1.Install (staNodes.Get (0));
  sta1App.Start (Seconds (1.0));
  sta1App.Stop (Seconds (simulationTime - 0.1));

  OnOffHelper onoff2 ("ns3::UdpSocketFactory", InetSocketAddress (ap2If.GetAddress (0), port2));
  onoff2.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff2.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  onoff2.SetAttribute ("Interval", StringValue (trafficInterval));
  ApplicationContainer sta2App = onoff2.Install (staNodes.Get (1));
  sta2App.Start (Seconds (1.0));
  sta2App.Stop (Seconds (simulationTime - 0.1));

  // Sink on AP1 and AP2
  PacketSinkHelper sink1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port1));
  ApplicationContainer sinkApp1 = sink1.Install (apNodes.Get (0));
  sinkApp1.Start (Seconds (0.0));
  sinkApp1.Stop (Seconds (simulationTime));

  PacketSinkHelper sink2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
  ApplicationContainer sinkApp2 = sink2.Install (apNodes.Get (1));
  sinkApp2.Start (Seconds (0.0));
  sinkApp2.Stop (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Throughput results
  uint64_t totalRx1 = DynamicCast<PacketSink> (sinkApp1.Get (0))->GetTotalRx ();
  uint64_t totalRx2 = DynamicCast<PacketSink> (sinkApp2.Get (0))->GetTotalRx ();
  double throughput1 = (totalRx1 * 8.0) / (simulationTime - 1.0) / 1e6; // Mbps
  double throughput2 = (totalRx2 * 8.0) / (simulationTime - 1.0) / 1e6; // Mbps

  std::cout << "BSS1 Throughput: " << throughput1 << " Mbps" << std::endl;
  std::cout << "BSS2 Throughput: " << throughput2 << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}