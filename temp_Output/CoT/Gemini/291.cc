#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("2s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobility.Install(nodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(4000);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpClientHelper client(interfaces.GetAddress(0), 4000);
  client.SetAttribute("MaxPackets", UintegerValue(20));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(5));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));

  // Pcap Tracing
  wifiPhy.EnablePcapAll("manet-aodv");

  // NetAnim
  // AnimationInterface anim("manet-aodv.xml");

  // FlowMonitor
  // FlowMonitorHelper flowmon;
  // Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  // monitor->CheckForLostPackets ();
  // Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  // std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  // for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  //   {
  // 	Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
  // 	std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
  // 	std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
  // 	std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
  // 	std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
  //   }

  // monitor->SerializeToXmlFile("manet-aodv.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}