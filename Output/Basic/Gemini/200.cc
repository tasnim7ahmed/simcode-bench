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
  nodes.Create(4);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue("50.0"),
                                 "Y", StringValue("50.0"),
                                 "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("50x50"));
  mobility.Install(nodes);

  UdpClientServerHelper echoClientServer(9);
  echoClientServer.SetAttribute("MaxPackets", UintegerValue(100));
  echoClientServer.SetAttribute("Interval", TimeValue(Seconds(1.)));
  echoClientServer.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClientServer.Install(nodes.Get(0), nodes.Get(3));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  ApplicationContainer serverApps = echoClientServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientServerHelper echoClientServer2(9);
  echoClientServer2.SetAttribute("MaxPackets", UintegerValue(100));
  echoClientServer2.SetAttribute("Interval", TimeValue(Seconds(1.)));
  echoClientServer2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = echoClientServer2.Install(nodes.Get(1), nodes.Get(2));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(10.0));

  ApplicationContainer serverApps2 = echoClientServer2.Install(nodes.Get(2));
  serverApps2.Start(Seconds(2.0));
  serverApps2.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("aodv-animation.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 10, 40);
  anim.SetConstantPosition(nodes.Get(2), 40, 10);
  anim.SetConstantPosition(nodes.Get(3), 40, 40);

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    std::cout << "  Average End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
  }

  Simulator::Destroy();

  return 0;
}