#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableRtsCts = false;
  uint32_t payloadSize = 1472;
  double simulationTime = 10;
  double distance = 10;

  CommandLine cmd;
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between AP and station", distance);
  cmd.Parse(argc, argv);

  NodeContainer apNodes;
  apNodes.Create(4);
  NodeContainer staNodes;
  staNodes.Create(4);

  YansWifiChannelHelper channel36 = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy36 = YansWifiPhyHelper::Default();
  phy36.SetChannel(channel36.Create());

  YansWifiChannelHelper channel40 = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy40 = YansWifiPhyHelper::Default();
  phy40.SetChannel(channel40.Create());

  YansWifiChannelHelper channel44 = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy44 = YansWifiPhyHelper::Default();
  phy44.SetChannel(channel44.Create());

  YansWifiChannelHelper channel48 = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy48 = YansWifiPhyHelper::Default();
  phy48.SetChannel(channel48.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ns-3-ssid-36");
  Ssid ssid2 = Ssid("ns-3-ssid-40");
  Ssid ssid3 = Ssid("ns-3-ssid-44");
  Ssid ssid4 = Ssid("ns-3-ssid-48");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(0.05)));
  NetDeviceContainer apDevice36 = wifi.Install(phy36, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(0.05)));
  NetDeviceContainer apDevice40 = wifi.Install(phy40, mac, apNodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid3),
              "BeaconInterval", TimeValue(Seconds(0.05)));
  NetDeviceContainer apDevice44 = wifi.Install(phy44, mac, apNodes.Get(2));

    mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid4),
              "BeaconInterval", TimeValue(Seconds(0.05)));
  NetDeviceContainer apDevice48 = wifi.Install(phy48, mac, apNodes.Get(3));


  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice36 = wifi.Install(phy36, mac, staNodes.Get(0));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice40 = wifi.Install(phy40, mac, staNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid3),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice44 = wifi.Install(phy44, mac, staNodes.Get(2));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid4),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice48 = wifi.Install(phy48, mac, staNodes.Get(3));

  if (enableRtsCts) {
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", StringValue("0"));
  }

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AggregationMode", StringValue("HtAggregatedMpdu"));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmsduSize", UintegerValue(7935));
  Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduSize", UintegerValue(65535));
  Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AggregationMode", StringValue("HtNonAggregated"));
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AggregationMode", StringValue("HtAggregatedMsdu"));
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmsduSize", UintegerValue(8191));
  Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AggregationMode", StringValue("HtTwoLevelAggregation"));
  Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduSize", UintegerValue(32767));
  Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmsduSize", UintegerValue(4095));

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

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(distance),
                                "MinY", DoubleValue(distance),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.Install(staNodes);


  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces36 = address.Assign(apDevice36);
  Ipv4InterfaceContainer staInterfaces36 = address.Assign(staDevice36);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces40 = address.Assign(apDevice40);
  Ipv4InterfaceContainer staInterfaces40 = address.Assign(staDevice40);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces44 = address.Assign(apDevice44);
  Ipv4InterfaceContainer staInterfaces44 = address.Assign(staDevice44);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces48 = address.Assign(apDevice48);
  Ipv4InterfaceContainer staInterfaces48 = address.Assign(staDevice48);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpClientHelper client36(staInterfaces36.GetAddress(0), port);
  client36.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client36.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
  client36.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApps36 = client36.Install(apNodes.Get(0));
  clientApps36.Start(Seconds(1.0));
  clientApps36.Stop(Seconds(simulationTime));

  UdpServerHelper server36(port);
  ApplicationContainer serverApps36 = server36.Install(staNodes.Get(0));
  serverApps36.Start(Seconds(0.0));
  serverApps36.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client40(staInterfaces40.GetAddress(0), port);
  client40.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client40.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
  client40.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApps40 = client40.Install(apNodes.Get(1));
  clientApps40.Start(Seconds(1.0));
  clientApps40.Stop(Seconds(simulationTime));

  UdpServerHelper server40(port);
  ApplicationContainer serverApps40 = server40.Install(staNodes.Get(1));
  serverApps40.Start(Seconds(0.0));
  serverApps40.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client44(staInterfaces44.GetAddress(0), port);
  client44.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client44.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
  client44.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApps44 = client44.Install(apNodes.Get(2));
  clientApps44.Start(Seconds(1.0));
  clientApps44.Stop(Seconds(simulationTime));

  UdpServerHelper server44(port);
  ApplicationContainer serverApps44 = server44.Install(staNodes.Get(2));
  serverApps44.Start(Seconds(0.0));
  serverApps44.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client48(staInterfaces48.GetAddress(0), port);
  client48.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client48.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
  client48.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApps48 = client48.Install(apNodes.Get(3));
  clientApps48.Start(Seconds(1.0));
  clientApps48.Stop(Seconds(simulationTime));

  UdpServerHelper server48(port);
  ApplicationContainer serverApps48 = server48.Install(staNodes.Get(3));
  serverApps48.Start(Seconds(0.0));
  serverApps48.Stop(Seconds(simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}