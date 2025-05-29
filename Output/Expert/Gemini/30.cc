#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t nSta = 3;
  uint32_t htMcs = 7;
  uint32_t channelWidth = 20;
  bool shortGuardInterval = true;
  double stationDistance = 5.0;
  bool rtsCtsEnabled = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("nSta", "Number of wifi STA devices", nSta);
  cmd.AddValue("htMcs", "HT MCS value (0-7)", htMcs);
  cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
  cmd.AddValue("stationDistance", "Distance between AP and stations", stationDistance);
  cmd.AddValue("rtsCtsEnabled", "Enable RTS/CTS", rtsCtsEnabled);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiTosSimulation", LOG_LEVEL_INFO);
  }

  Config::SetDefault ("ns3::WifiMac::BE_CwMin", UintegerValue (15));
  Config::SetDefault ("ns3::WifiMac::BE_CwMax", UintegerValue (1023));
  Config::SetDefault ("ns3::WifiMac::BE_Aifs", UintegerValue (3));
  Config::SetDefault ("ns3::WifiMac::VI_CwMin", UintegerValue (7));
  Config::SetDefault ("ns3::WifiMac::VI_CwMax", UintegerValue (63));
  Config::SetDefault ("ns3::WifiMac::VI_Aifs", UintegerValue (2));
  Config::SetDefault ("ns3::WifiMac::VO_CwMin", UintegerValue (3));
  Config::SetDefault ("ns3::WifiMac::VO_CwMax", UintegerValue (31));
  Config::SetDefault ("ns3::WifiMac::VO_Aifs", UintegerValue (1));
  Config::SetDefault ("ns3::WifiMac::AC_BE_TxopLimit", TimeValue (Seconds (0)));
  Config::SetDefault ("ns3::WifiMac::AC_VI_TxopLimit", TimeValue (Seconds (0.006)));
  Config::SetDefault ("ns3::WifiMac::AC_VO_TxopLimit", TimeValue (Seconds (0.003)));

  NodeContainer staNodes;
  staNodes.Create(nSta);
  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  Wifi80211nHelper wifi80211n;
  if (rtsCtsEnabled) {
      wifi80211n.SetRtsCtsThreshold(0);
  }
  wifi80211n.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue("HtMcs" + std::to_string(htMcs)),
                                      "ControlMode", StringValue("HtMcs0"));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi80211n.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "QosSupported", BooleanValue(true));

  NetDeviceContainer apDevices = wifi80211n.Install(phy, mac, apNode);

  if (channelWidth == 40) {
      wifi80211n.SetHtConfiguration(HtWifiMac::HT_CHANNEL_WIDTH_40MHZ, shortGuardInterval);
  } else {
      wifi80211n.SetHtConfiguration(HtWifiMac::HT_CHANNEL_WIDTH_20MHZ, shortGuardInterval);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(stationDistance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(nSta),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  apNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, stationDistance * (nSta + 1), 0.0));

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < nSta; ++i) {
      PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(staNodeInterfaces.GetAddress(i), port));
      sinkApps.Add(sink.Install(staNodes.Get(i)));
  }
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nSta; ++i) {
      UdpClientHelper client(staNodeInterfaces.GetAddress(i), port);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295));
      client.SetAttribute("Interval", TimeValue(Time("ns", 100)));
      client.SetAttribute("PacketSize", UintegerValue(1024));
      client.SetAttribute("Tos", UintegerValue(i));
      clientApps.Add(client.Install(apNode.Get(0)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughput = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
      totalThroughput += throughput;
  }

  std::cout << "Aggregated UDP Throughput: " << totalThroughput << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}