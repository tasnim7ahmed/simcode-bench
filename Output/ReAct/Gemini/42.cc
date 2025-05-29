#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  bool rtsCtsEnabled = false;
  uint32_t numAggregatedMpdu = 64;
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double minThrouput = 10;

  CommandLine cmd;
  cmd.AddValue("rtsCts", "Enable RTS/CTS (0 or 1)", rtsCtsEnabled);
  cmd.AddValue("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("minThrouput", "Minimum throughput in Mbps", minThrouput);
  cmd.Parse(argc, argv);

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNodes);

  if (rtsCtsEnabled) {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/RtsCtsThreshold", StringValue ("0"));
  }

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmpduSize", UintegerValue (numAggregatedMpdu));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmsduSize", UintegerValue (7935));

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
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper echoClient(apInterface.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
  echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps1 = echoClient.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime + 1));

  ApplicationContainer clientApps2 = echoClient.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime + 1));


  phy.EnablePcap("hidden", apDevice);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughput = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == apInterface.GetAddress(0)) {
        double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        totalThroughput += throughput;
    }
  }

  Simulator::Destroy();

  if (totalThroughput < minThrouput) {
      std::cerr << "Error: Achieved throughput " << totalThroughput << " Mbps is less than minimum required " << minThrouput << " Mbps" << std::endl;
      return 1;
  } else {
    std::cout << "Average throughput: " << totalThroughput << " Mbps" << std::endl;
    return 0;
  }
}