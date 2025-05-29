#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[]) {

  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue("EnablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetMinimumLogLevel(LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingProtocol(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue("100.0"),
                                 "Y", StringValue("100.0"),
                                 "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 200, 0, 200)));
  mobility.Install(nodes);

  UdpClientServerHelper cbr(interfaces.GetAddress(0), 9);
  cbr.SetAttribute("MaxBytes", UintegerValue(0));
  cbr.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = cbr.GetClient();
  clientApp.SetStartTime(Seconds(1.0));
  clientApp.SetStopTime(Seconds(10.0));

  ApplicationContainer serverApp = cbr.GetServer();
  serverApp.SetStartTime(Seconds(0.0));
  serverApp.SetStopTime(Seconds(10.0));

  UdpClientServerHelper cbr1(interfaces.GetAddress(1), 9);
  cbr1.SetAttribute("MaxBytes", UintegerValue(0));
  cbr1.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp1 = cbr1.GetClient();
  clientApp1.SetStartTime(Seconds(2.0));
  clientApp1.SetStopTime(Seconds(10.0));

  ApplicationContainer serverApp1 = cbr1.GetServer();
  serverApp1.SetStartTime(Seconds(0.0));
  serverApp1.SetStopTime(Seconds(10.0));

  UdpClientServerHelper cbr2(interfaces.GetAddress(2), 9);
  cbr2.SetAttribute("MaxBytes", UintegerValue(0));
  cbr2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp2 = cbr2.GetClient();
  clientApp2.SetStartTime(Seconds(3.0));
  clientApp2.SetStopTime(Seconds(10.0));

  ApplicationContainer serverApp2 = cbr2.GetServer();
  serverApp2.SetStartTime(Seconds(0.0));
  serverApp2.SetStopTime(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (enablePcap) {
    phy.EnablePcapAll("aodv");
  }

  AnimationInterface anim("aodv.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 50, 50);
  anim.SetConstantPosition(nodes.Get(2), 100, 100);
  anim.SetConstantPosition(nodes.Get(3), 150, 150);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    std::cout << "  End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
  }

  Simulator::Destroy();
  return 0;
}