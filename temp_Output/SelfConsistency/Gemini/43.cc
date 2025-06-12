#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenNodeMpduAggregation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  bool pcapTracing = false;
  std::string animFile = "wifi-hidden-node-mpdu-aggregation.xml";
  uint32_t nMpdu = 64;  // Number of aggregated MPDUs
  uint32_t payloadSize = 1472; // UDP payload size
  double simulationTime = 10; // Simulation time in seconds
  bool enableRtsCts = true;
  double expectedThroughputMbps = 50.0; // Expected throughput in Mbps
  double throughputTolerance = 0.1;      // Tolerance for throughput verification (10%)

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("pcapTracing", "Enable pcap tracing", pcapTracing);
  cmd.AddValue("animFile", "File Name for Animation Output", animFile);
  cmd.AddValue("nMpdu", "Number of aggregated MPDUs", nMpdu);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughputMbps);
    cmd.AddValue("throughputTolerance", "Tolerance for throughput verification (fraction)", throughputTolerance);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiHiddenNodeMpduAggregation", LOG_LEVEL_INFO);
  }

  Config::SetDefault("ns3::WifiMacQueue::MaxMpdu", UintegerValue(nMpdu));

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "EnableBeaconJitter", BooleanValue(false));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  if (enableRtsCts) {
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", StringValue("0"));
  } else {
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", StringValue("2200"));
  }

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);
  mobility.Install(apNode);

  Ptr<Node> ap = apNode.Get(0);
  Ptr<MobilityModel> mobilityModel = ap->GetObject<MobilityModel>();
  Vector position = Vector(10.0, 0.0, 0.0);
  mobilityModel->SetPosition(position);


  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  staNodeInterface = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterface;
  apNodeInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5000;

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                       InetSocketAddress(apNodeInterface.GetAddress(0), port));
  ApplicationContainer sinkApp = sink.Install(apNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client(apNodeInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.00001"))); // 10 usec; send packets as fast as possible
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps;
  clientApps.Add(client.Install(staNodes.Get(0)));
  clientApps.Add(client.Install(staNodes.Get(1)));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (tracing) {
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden-node-mpdu-aggregation.tr"));
  }

  if (pcapTracing) {
    phy.EnablePcapAll("wifi-hidden-node-mpdu-aggregation");
  }

  AnimationInterface anim(animFile);
  anim.SetConstantPosition(apNode.Get(0), 10.0, 0.0);
  anim.SetConstantPosition(staNodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(staNodes.Get(1), 5.0, 0.0);

  Simulator::Stop(Seconds(simulationTime + 2));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalRxBytes = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_INFO("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
    NS_LOG_INFO("  Tx Bytes:   " << i->second.txBytes);
    NS_LOG_INFO("  Rx Bytes:   " << i->second.rxBytes);
    NS_LOG_INFO("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
    totalRxBytes += i->second.rxBytes;
  }

  double actualThroughputMbps = totalRxBytes * 8.0 / simulationTime / 1000000;
  std::cout << "Actual throughput: " << actualThroughputMbps << " Mbps" << std::endl;

  double lowerBound = expectedThroughputMbps * (1 - throughputTolerance);
  double upperBound = expectedThroughputMbps * (1 + throughputTolerance);

  if (actualThroughputMbps >= lowerBound && actualThroughputMbps <= upperBound) {
        std::cout << "Throughput verification: PASS" << std::endl;
    } else {
        std::cout << "Throughput verification: FAIL" << std::endl;
        std::cout << "Expected throughput range: [" << lowerBound << ", " << upperBound << "] Mbps" << std::endl;
  }

  monitor->SerializeToXmlFile("wifi-hidden-node-mpdu-aggregation.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}