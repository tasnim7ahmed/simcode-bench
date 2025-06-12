#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool rtsCtsEnabled = false;
  uint32_t maxAmpduSize = 1;
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double expectedThroughputMbps = 50.0;

  CommandLine cmd;
  cmd.AddValue("rtsCtsEnabled", "Enable/disable RTS/CTS (0=disable, 1=enable)", rtsCtsEnabled);
  cmd.AddValue("maxAmpduSize", "Maximum number of aggregated MPDUs", maxAmpduSize);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughputMbps);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue(maxAmpduSize));

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("Distance", DoubleValue(5));

  WifiHelper wifi = WifiHelper::Default();
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  if (rtsCtsEnabled) {
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", StringValue("0"));
  } else {
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", StringValue("2200"));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNodes);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper echoClient(apInterface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
  echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps1 = echoClient.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(1.0));
  clientApps1.Stop(Seconds(simulationTime + 1));

   ApplicationContainer clientApps2 = echoClient.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(1.0));
  clientApps2.Stop(Seconds(simulationTime + 1));

  phy.EnablePcapAll("wifi-hidden-node-aggregation");
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden-node-aggregation.tr"));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalRxBytes = 0.0;
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == apInterface.GetAddress(0)) {
       totalRxBytes += i->second.rxBytes;
    }
  }

  double throughput = (totalRxBytes * 8) / (simulationTime * 1000000.0);

  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  if (throughput >= expectedThroughputMbps) {
    std::cout << "Throughput test passed!" << std::endl;
  } else {
    std::cout << "Throughput test failed!" << std::endl;
  }

  monitor->SerializeToXmlFile("wifi-hidden-node-aggregation.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}