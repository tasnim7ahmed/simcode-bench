#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenStation");

int main(int argc, char *argv[]) {
  bool rtsCtsEnabled = false;
  uint32_t numAggregatedMpdu = 3;
  double simulationTime = 10.0;
  uint32_t payloadSize = 1472; //default value of a typical MTU size
  double minThrouput = 5.0; // Mbps
  double maxThrouput = 15.0; // Mbps

  CommandLine cmd;
  cmd.AddValue("rtsCts", "Enable RTS/CTS (0 or 1)", rtsCtsEnabled);
  cmd.AddValue("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
  cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("minThrouput", "Minimum expected throuput (Mbps)", minThrouput);
  cmd.AddValue("maxThrouput", "Maximum expected throuput (Mbps)", maxThrouput);

  cmd.Parse(argc, argv);

  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // AP, station1, station2

  NodeContainer staNodes = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer apNode = NodeContainer(nodes.Get(0));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  // Configure AP
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "QosSupported", BooleanValue(true),
              "HtSupported", BooleanValue(true));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configure Stations
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "QosSupported", BooleanValue(true),
              "HtSupported", BooleanValue(true));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  // Enable or disable RTS/CTS
  if (rtsCtsEnabled) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmsduLength", UintegerValue(7935));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AggregationSupported", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MpduAggregationMaxAmpduLength", UintegerValue(65535));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MpduAggregationMaxNumMpdu", UintegerValue(numAggregatedMpdu));
    Config::SetDefault("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue(65535));
    Config::SetDefault("ns3::WifiMacQueue::MaxAmpduPackets", UintegerValue(numAggregatedMpdu));
    Config::SetDefault("ns3::WifiMacQueue::EnableRts", BooleanValue(true));
    Config::SetDefault("ns3::WifiMacQueue::RtsThreshold", UintegerValue(0));
  } else {
    Config::SetDefault("ns3::WifiMacQueue::EnableRts", BooleanValue(false));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Set positions: AP at (0,0), station1 at (5,10), station2 at (10,10) which are out of each other's range.
  Ptr<ConstantPositionMobilityModel> apMobility = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
  Ptr<ConstantPositionMobilityModel> sta1Mobility = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  sta1Mobility->SetPosition(Vector(5.0, 10.0, 0.0));
  Ptr<ConstantPositionMobilityModel> sta2Mobility = nodes.Get(2)->GetObject<ConstantPositionMobilityModel>();
  sta2Mobility->SetPosition(Vector(10.0, 10.0, 0.0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevices));

  uint16_t port = 9; // Discard port
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(apNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client1(interfaces.GetAddress(0), port); // Send to AP
  client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client1.SetAttribute("Interval", TimeValue(Time("ns3::Seconds", 0.001))); // packets/sec
  client1.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime));

  UdpClientHelper client2(interfaces.GetAddress(0), port); // Send to AP
  client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client2.SetAttribute("Interval", TimeValue(Time("ns3::Seconds", 0.001))); // packets/sec
  client2.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(simulationTime + 1));

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Enable Tracing
  phy.EnablePcapAll("wifi-hidden-station");

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden-station.tr"));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  double totalThroughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
    NS_LOG_UNCOND("  Tx Bytes:   " << i->second.txBytes);
    NS_LOG_UNCOND("  Rx Bytes:   " << i->second.rxBytes);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
    totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
  }

  NS_LOG_UNCOND("Total Throughput: " << totalThroughput << " Mbps");

  if (totalThroughput >= minThrouput && totalThroughput <= maxThrouput )
  {
      NS_LOG_UNCOND("Test PASSED");
  } else {
      NS_LOG_UNCOND("Test FAILED");
  }

  monitor->SerializeToXmlFile("wifi-hidden-station.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}