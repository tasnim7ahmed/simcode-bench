#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  std::string phyMode("DsssRate1Mbps");
  double distance = 1.0;
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1;
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  bool pcapTracing = false;
  bool asciiTracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue("sourceNode", "Sender node number", sourceNode);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue("ascii", "Enable ASCII tracing", asciiTracing);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("adhoc-grid")));

  NodeContainer nodes;
  nodes.Create(25);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(-10));
  wifiPhy.Set("TxGain", DoubleValue(0));
  wifiPhy.Set("CcaEdThreshold", DoubleValue(-79));
  wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-76));
  wifiPhy.Set("RxNoiseFigure", DoubleValue(7));
  wifiPhy.Set("TxPowerStart", DoubleValue(16.0206));
  wifiPhy.Set("TxPowerEnd", DoubleValue(16.0206));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(-80));
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(distance),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(olsr);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(sinkNode));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(i.GetAddress(sinkNode), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(sourceNode));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  if (pcapTracing) {
    wifiPhy.EnablePcapAll("adhoc-grid");
  }
  if (asciiTracing) {
    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("adhoc-grid.tr"));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
  }

  Simulator::Destroy();

  return 0;
}