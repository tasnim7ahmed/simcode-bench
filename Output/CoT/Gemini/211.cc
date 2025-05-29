#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/olsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExample");

int main(int argc, char *argv[]) {
  bool enableFlowMonitor = true;
  std::string phyMode("OfdmRate6MbpsBW10MHz");
  double simulationTime = 10;
  std::string traceFile = "adhoc-wifi.pcap";

  CommandLine cmd(__FILE__);
  cmd.AddValue("EnableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("traceFile", "Pcap trace file name", traceFile);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1));

  UdpEchoClientHelper echoClient(i.GetAddress(4), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 2));

  if (enableFlowMonitor) {
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("adhoc-wifi.flowmon", true, true);
  } else {
      Simulator::Stop(Seconds(simulationTime));
      Simulator::Run();
  }

  Simulator::Destroy();
  return 0;
}