#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool enableObssPd = true;
  double d1 = 10.0;
  double d2 = 20.0;
  double d3 = 30.0;
  double txPowerDbm = 20.0;
  int packetSize = 1472;
  double interval = 0.001;
  double simulationTime = 10.0;
  double ccaEdThresholdDbm = -82.0;
  double obssPdThresholdDbm = -62.0;
  int channelWidth = 20;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if possible", verbose);
  cmd.AddValue("enableObssPd", "Enable OBSS PD spatial reuse", enableObssPd);
  cmd.AddValue("d1", "Distance STA1-AP1", d1);
  cmd.AddValue("d2", "Distance AP1-STA2", d2);
  cmd.AddValue("d3", "Distance AP2-STA1", d3);
  cmd.AddValue("txPowerDbm", "Transmit power in dBm", txPowerDbm);
  cmd.AddValue("packetSize", "Size of packets", packetSize);
  cmd.AddValue("interval", "Interpacket interval", interval);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("ccaEdThresholdDbm", "CCA ED threshold in dBm", ccaEdThresholdDbm);
  cmd.AddValue("obssPdThresholdDbm", "OBSS PD threshold in dBm", obssPdThresholdDbm);
  cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();

  phy.SetChannel(channel.Create());

  WifiHelper wifi = WifiHelper::Default();
  wifi.SetStandard(WIFI_STANDARD_80211ax);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ns-3-ssid-1");
  Ssid ssid2 = Ssid("ns-3-ssid-2");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevs1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevs2 = wifi.Install(phy, mac, apNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevs1 = wifi.Install(phy, mac, staNodes.Get(0));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevs2 = wifi.Install(phy, mac, staNodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(apNodes);
  mobility.Install(staNodes);

  Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  sta1Mobility->SetPosition(Vector(0.0, 0.0, 0.0));
  Ptr<ConstantPositionMobilityModel> ap1Mobility = apNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  ap1Mobility->SetPosition(Vector(d1, 0.0, 0.0));
  Ptr<ConstantPositionMobilityModel> sta2Mobility = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  sta2Mobility->SetPosition(Vector(d1 + d2 + d3, 0.0, 0.0));
  Ptr<ConstantPositionMobilityModel> ap2Mobility = apNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  ap2Mobility->SetPosition(Vector(d1 + d2, 0.0, 0.0));

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staApInterface1 = address.Assign(staDevs1);
  address.Assign(apDevs1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staApInterface2 = address.Assign(staDevs2);
  address.Assign(apDevs2);

  UdpServerHelper server1(9);
  ApplicationContainer serverApps1 = server1.Install(apNodes.Get(0));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client1(Ipv4Address("10.1.1.2"), 9);
  client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client1.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client1.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime));

  UdpServerHelper server2(9);
  ApplicationContainer serverApps2 = server2.Install(apNodes.Get(1));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client2(Ipv4Address("10.1.2.2"), 9);
  client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client2.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client2.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime));

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelWidth));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerDbm));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerDbm));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/CcaEdThreshold", DoubleValue(ccaEdThresholdDbm));
  if (enableObssPd) {
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ObssPdThreshold", DoubleValue(obssPdThresholdDbm));
  } else {
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ObssPdThreshold", DoubleValue(-999.0));
  }
  phy.EnableAsciiAll("spatial_reuse.tr");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double throughput1 = 0.0;
  double throughput2 = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == Ipv4Address("10.1.1.2")) {
      throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
    } else if (t.destinationAddress == Ipv4Address("10.1.2.2")) {
      throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
    }
  }

  Simulator::Destroy();

  std::cout << "Throughput BSS1: " << throughput1 << " Mbps" << std::endl;
  std::cout << "Throughput BSS2: " << throughput2 << " Mbps" << std::endl;

  return 0;
}