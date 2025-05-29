#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[]) {

  bool enablePcap = true;
  std::string phyMode("DsssRate11Mbps");

  CommandLine cmd;
  cmd.AddValue("EnablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::Ssid", StringValue("adhoc-network"));

  NodeContainer nodes;
  nodes.Create(10);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(-10));
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("2s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install(nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<MobilityModel> mob = nodes.Get(i)->GetObject<MobilityModel>();
      Simulator::Schedule(Seconds(0.1), &MobilityModel::SetPosition, mob, Vector(i*10, i*10, 0));
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(29.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    clientApps = echoClient.Install(nodes.Get(i));
    clientApps.Start(Seconds(2.0 + i*0.1));
    clientApps.Stop(Seconds(28.0));
  }

  if (enablePcap) {
    wifiPhy.EnablePcapAll("aodv-adhoc");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(30.0));

  AnimationInterface anim("aodv-adhoc.xml");

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelay = 0;
  int flowCount = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

    totalPacketsSent += i->second.sentPackets;
    totalPacketsReceived += i->second.rxPackets;
    totalDelay += i->second.delaySum.GetSeconds();
    flowCount++;

    std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
    std::cout << "  Tx Packets: " << i->second.sentPackets << std::endl;
    std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
    std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
    std::cout << "  Delay Sum: " << i->second.delaySum << std::endl;
  }

  double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
  double averageEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelay / totalPacketsReceived) : 0;

  std::cout << "\nSimulation Results:" << std::endl;
  std::cout << "  Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "  Average End-to-End Delay: " << averageEndToEndDelay << " seconds" << std::endl;

  Simulator::Destroy();
  return 0;
}