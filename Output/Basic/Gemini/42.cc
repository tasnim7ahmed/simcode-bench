#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableRtsCts = false;
  uint32_t numAggregatedMpdu = 2;
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double minExpectedThroughput = 5.0;
  std::string dataRate = "OfdmRate54Mbps";
  std::string phyMode = "HtMcs7";

  CommandLine cmd;
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS (0 or 1)", enableRtsCts);
  cmd.AddValue("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("minExpectedThroughput", "Minimum expected throughput in Mbps", minExpectedThroughput);
  cmd.AddValue("dataRate", "Data rate for UDP traffic", dataRate);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue(65535));
  Config::SetDefault("ns3::WifiMacQueue::MaxAmsduSize", UintegerValue(7935));
  Config::SetDefault("ns3::WifiMacQueue::FragmentationThreshold", UintegerValue(2346));

  Config::SetDefault("ns3::WifiMacQueue::Cwmin", UintegerValue(15));
  Config::SetDefault("ns3::WifiMacQueue::Cwmax", UintegerValue(1023));

  Config::SetDefault("ns3::WifiMacQueue::MpduAggregationMax", UintegerValue(numAggregatedMpdu));
  Config::SetDefault("ns3::WifiMacQueue::EnableBlockAck", BooleanValue(true));

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNodes;
  staNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(MilliSeconds(100)));

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  if (enableRtsCts) {
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", UintegerValue(0));
  } else {
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", UintegerValue(2347));
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
  mobility.Install(apNode);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(1.0),
                                "MinY", DoubleValue(1.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes.Get(0));

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(1.0),
                                "MinY", DoubleValue(1000.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes.Get(1));


  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(NetDeviceContainer::Create(apDevice, staDevices));

  Ipv4Address serverAddress = i.GetAddress(0);

  uint16_t port = 9;

  UdpClientHelper client(serverAddress, port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time(Seconds(0.00002))));
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps;
  clientApps.Add(client.Install(staNodes.Get(0)));
  clientApps.Add(client.Install(staNodes.Get(1)));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(apNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(simulationTime));

  phyMode = "HtMcs7";
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", EnumValue (HT_GI_800NS));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (true));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PreambleDetectionTimeout", TimeValue (NanoSeconds (2)));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MaxSupportedTxSpatialStreams", UintegerValue (1));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MaxSupportedRxSpatialStreams", UintegerValue (1));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxSpatialStreams", UintegerValue (1));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxSpatialStreams", UintegerValue (1));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode",StringValue (phyMode),
                                    "ControlMode",StringValue (phyMode));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalRxBytes = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

      if (t.destinationAddress == serverAddress) {
          totalRxBytes += i->second.rxBytes;
          double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;

          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Throughput: " << throughput << " Mbps\n";

          NS_TEST_ASSERT_MSG_FIRST (throughput >= minExpectedThroughput, "TcpClient: Throughput " << throughput << " Mbps below minimum of " << minExpectedThroughput << " Mbps");
      }
  }

  monitor->SerializeToXmlFile("hidden-stations.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}