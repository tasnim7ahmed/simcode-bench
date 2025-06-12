#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1;
  std::string phyMode("DsssRate11Mbps");
  double distance = 1.0;
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  bool pcapTracing = false;
  bool asciiTracing = false;
  uint32_t gridSide = 5;

  CommandLine cmd(__FILE__);
  cmd.AddValue("verbose", "Tell application to log if verbose", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distance", "Grid distance", distance);
  cmd.AddValue("sinkNode", "Sink node number", sinkNode);
  cmd.AddValue("sourceNode", "Source node number", sourceNode);
  cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue("asciiTracing", "Enable ASCII tracing", asciiTracing);
  cmd.AddValue("gridSide", "Number of nodes on one side of the grid", gridSide);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(gridSide * gridSide);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(-10));
  wifiPhy.Set("TxGain", DoubleValue(10));
  wifiPhy.Set("CcaEdThreshold", DoubleValue(-95));
  wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-95));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100));

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
                                "GridWidth", UintegerValue(gridSide),
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

  if (tracing) {
    PointToPointHelper pointToPoint;
    pointToPoint.EnablePcapAll("adhoc");
  }

  if (pcapTracing) {
    wifiPhy.EnablePcapAll("adhoc");
  }

  if (asciiTracing) {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll(ascii.CreateFileStream("adhoc.tr"));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("adhoc.xml");

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
  }

  Simulator::Destroy();
  return 0;
}