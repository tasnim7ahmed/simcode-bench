#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpMpduAggregationExample");

struct ThroughputCounter
{
  uint64_t lastTotalRx;
  Time lastTime;
  Ptr<Application> app;
  Ptr<PacketSink> sink;
};

void
ThroughputCallback(ThroughputCounter *counter)
{
  Time curTime = Simulator::Now();
  uint64_t curTotalRx = counter->sink->GetTotalRx();
  double throughput = (curTotalRx - counter->lastTotalRx) * 8.0 / (curTime - counter->lastTime).GetSeconds() / 1e6;
  std::cout << std::fixed << std::setprecision(3)
            << "Time: " << curTime.GetSeconds() << "s, Throughput: "
            << throughput << " Mbps" << std::endl;
  counter->lastTotalRx = curTotalRx;
  counter->lastTime = curTime;
  Simulator::Schedule(MilliSeconds(100), &ThroughputCallback, counter);
}

int
main(int argc, char *argv[])
{
  // Default parameters
  std::string tcpCongCtrl = "ns3::TcpNewReno";
  std::string dataRate = "100Mbps";
  uint32_t payloadSize = 1460;
  std::string phyBitrate = "150Mbps";
  double simulationTime = 10.0;
  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue("tcpCongCtrl", "TCP congestion control algorithm", tcpCongCtrl);
  cmd.AddValue("dataRate", "Application data rate", dataRate);
  cmd.AddValue("payloadSize", "Application payload size (bytes)", payloadSize);
  cmd.AddValue("phyBitrate", "Wi-Fi physical layer bitrate", phyBitrate);
  cmd.AddValue("simulationTime", "Simulation duration in seconds", simulationTime);
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpCongCtrl));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
  Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue("HtMcs7"));
  Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue("HtMcs0"));

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  WifiMacHelper mac;

  // MPDU aggregation enabled
  HtWifiMacHelper htMac;
  HtConfiguration htConf;
  htConf.SetMaxAmpduSize(65535); // maximum A-MPDU size
  htConf.SetMaxAmsduSize(7935); // optional, but enables AMSDU too
  // Enable MPDU aggregation
  Config::SetDefault("ns3::WifiRemoteStationManager::EnableAmsdu", BooleanValue(true));

  Ssid ssid = Ssid("ns3-80211n");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  phy.Set("TxPowerStart", DoubleValue(20.0));
  phy.Set("TxPowerEnd", DoubleValue(20.0));

  NetDeviceContainer staDevice, apDevice;
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  // Set HtMac configuration (including enabling AMPDU aggregation)
  for (uint32_t i = 0; i < staDevice.GetN(); ++i)
  {
    Ptr<NetDevice> nd = staDevice.Get(i);
    Ptr<WifiNetDevice> wnd = DynamicCast<WifiNetDevice>(nd);
    Ptr<HtWifiMac> htmac = DynamicCast<HtWifiMac>(wnd->GetMac());
    if (htmac)
    {
      htmac->SetHtConfiguration(htConf);
    }
  }
  for (uint32_t i = 0; i < apDevice.GetN(); ++i)
  {
    Ptr<NetDevice> nd = apDevice.Get(i);
    Ptr<WifiNetDevice> wnd = DynamicCast<WifiNetDevice>(nd);
    Ptr<HtWifiMac> htmac = DynamicCast<HtWifiMac>(wnd->GetMac());
    if (htmac)
    {
      htmac->SetHtConfiguration(htConf);
    }
  }

  // Enforce selected PHY bitrate (mcs)
  phy.Set("ShortGuardEnabled", BooleanValue(true));
  Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue("HtMcs7"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(0.0),
                               "GridWidth", UintegerValue(1),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(5.0));
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterface, apInterface;
  staInterface = address.Assign(staDevice);
  apInterface = address.Assign(apDevice);

  uint16_t port = 50000;
  Address apLocalAddress(InetSocketAddress(apInterface.GetAddress(0), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", apLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(wifiApNode);

  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime + 0.1));

  OnOffHelper clientHelper("ns3::TcpSocketFactory", apLocalAddress);
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
  ApplicationContainer clientApp = clientHelper.Install(wifiStaNode);

  // Enable PCAP if specified
  if (enablePcap)
  {
    phy.EnablePcap("tcp80211n_mpdu_aggregation_ap", apDevice.Get(0));
    phy.EnablePcap("tcp80211n_mpdu_aggregation_sta", staDevice.Get(0));
  }

  // Throughput measurement
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
  ThroughputCounter *counter = new ThroughputCounter{0, Seconds(1.0), clientApp.Get(0), sink};
  Simulator::Schedule(Seconds(1.1), &ThroughputCallback, counter);

  // Run
  Simulator::Stop(Seconds(simulationTime + 0.1));
  Simulator::Run();

  // Output final throughput
  double totalThroughput = sink->GetTotalRx() * 8.0 / simulationTime / 1e6;
  std::cout << "Average Throughput: " << totalThroughput << " Mbps" << std::endl;

  Simulator::Destroy();
  delete counter;
  return 0;
}