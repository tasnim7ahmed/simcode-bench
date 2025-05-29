#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(nodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  UdpClientHelper client(interfaces.GetAddress(4), 9);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  phy.EnablePcapAll("aodv");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

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
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("aodv.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}