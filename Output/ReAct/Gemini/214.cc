#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = true;
  uint32_t payloadSize = 1472;
  std::string dataRate = "54Mbps";
  std::string delay = "2ms";
  uint64_t simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("dataRate", "Data rate for the UDP client", dataRate);
  cmd.AddValue("delay", "UDP client packet inter-arrival delay", delay);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer wifiNodes;
  wifiNodes.Create(4);

  NodeContainer staNodes;
  staNodes.Add(wifiNodes.Get(1));
  staNodes.Add(wifiNodes.Get(2));
  staNodes.Add(wifiNodes.Get(3));
  NodeContainer apNode;
  apNode.Add(wifiNodes.Get(0));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  InternetStackHelper internet;
  internet.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(staNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpClientHelper client(staNodeInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(2)));
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps1 = client.Install(staNodes.Get(1));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime));

  ApplicationContainer clientApps2 = client.Install(staNodes.Get(2));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime));

  if (tracing) {
    phy.EnablePcap("wifi-example", apDevices.Get(0));
    phy.EnablePcap("wifi-example", staDevices);
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}