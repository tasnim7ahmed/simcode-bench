/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * TCP over 802.11n with MPDU Aggregation
 * One AP and one STA, STA sends TCP data to AP.
 * Reports throughput every 100 ms and supports parameter configuration.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiMpduAggregationExample");

class ThroughputMonitor
{
public:
  ThroughputMonitor(Ptr<Application> app, Time interval)
    : m_app (DynamicCast<BulkSendApplication>(app)),
      m_interval (interval),
      m_lastTotalBytes (0)
  {
    m_output.open ("throughput.log", std::ios::out);
    Simulator::Schedule(m_interval, &ThroughputMonitor::ReportThroughput, this);
  }

  ~ThroughputMonitor()
  {
    m_output.close();
  }

  void ReportThroughput()
  {
    uint64_t totalBytes = m_app->GetTotalBytes();
    double throughput = (totalBytes - m_lastTotalBytes) * 8.0 / m_interval.GetSeconds() / 1e6; // Mbps
    m_lastTotalBytes = totalBytes;
    double now = Simulator::Now().GetSeconds();
    m_output << now << "s: " << throughput << " Mbps" << std::endl;
    std::cout << now << "s: " << throughput << " Mbps" << std::endl;
    Simulator::Schedule(m_interval, &ThroughputMonitor::ReportThroughput, this);
  }

private:
  Ptr<BulkSendApplication> m_app;
  Time m_interval;
  uint64_t m_lastTotalBytes;
  std::ofstream m_output;
};

int
main (int argc, char *argv[])
{
  // Configurable parameters
  std::string tcpType = "ns3::TcpNewReno";
  std::string dataRate = "50Mbps";
  std::string phyRate = "150Mbps";
  uint32_t payloadSize = 1448;
  double simTime = 10.0;
  bool enablePcap = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue ("tcpType", "TCP congestion control: ns3::TcpNewReno, ns3::TcpCubic, etc.", tcpType);
  cmd.AddValue ("dataRate", "Application data rate (e.g. 50Mbps)", dataRate);
  cmd.AddValue ("phyRate", "Wi-Fi physical bitrate (e.g. 150Mbps)", phyRate);
  cmd.AddValue ("payloadSize", "TCP payload size in bytes", payloadSize);
  cmd.AddValue ("simTime", "Simulation duration (seconds)", simTime);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  // Set TCP Congestion Control
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpType));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  // Create Nodes
  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Wi-Fi config
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  // Enable MPDU Aggregation (A-MPDU)
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("HtMcs7"), // 150Mbps
                               "ControlMode", StringValue ("HtMcs0"));

  // MPDU aggregation settings
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200")); // Disable RTS/CTS
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200")); // No fragmentation
  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue("1000p"));
  Config::SetDefault ("ns3::WifiPhy::ShortGuardEnabled", BooleanValue (true));
  Config::SetDefault ("ns3::HtWifiMac::MpduAggregation", BooleanValue (true));
  Config::SetDefault ("ns3::HtWifiMac::MsduAggregation", BooleanValue (false));
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxAmsduSize", UintegerValue (7935));
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue (65535));

  Ssid ssid = Ssid ("ns3-80211n");

  WifiMacHelper mac;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Set Wi-Fi MCS and Tx power
  phy.Set ("TxPowerStart", DoubleValue (20.0));
  phy.Set ("TxPowerEnd", DoubleValue (20.0));
  phy.Set ("ChannelWidth", UintegerValue (40));
  phy.Set ("ShortGuardEnabled", BooleanValue (true));

  // Set PHY bitrate (overwriting DataMode if necessary)
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue ("HtMcs7"));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  pos->Add (Vector (0.0, 0.0, 0.0)); // STA
  pos->Add (Vector (5.0, 0.0, 0.0)); // AP
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface, apInterface;
  staInterface = address.Assign (staDevice);
  apInterface = address.Assign (apDevice);

  // TCP application: STA -> AP
  uint16_t port = 5000;
  Address sinkAddress (InetSocketAddress (apInterface.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (wifiApNode);

  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime + 1));

  BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  source.SetAttribute ("SendSize", UintegerValue (payloadSize));
  source.SetAttribute ("Remote", AddressValue (sinkAddress));
  ApplicationContainer sourceApp = source.Install (wifiStaNode);
  sourceApp.Start (Seconds (0.1));
  sourceApp.Stop (Seconds (simTime + 1));

  // Set up throughput reporting
  Ptr<Application> bulkApp = sourceApp.Get (0);
  ThroughputMonitor monitor (bulkApp, MilliSeconds (100));

  // PCAP tracing
  if (enablePcap)
    {
      phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      phy.EnablePcap ("sta", staDevice.Get (0), true, true);
      phy.EnablePcap ("ap", apDevice.Get (0), true, true);
    }

  Simulator::Stop (Seconds (simTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}