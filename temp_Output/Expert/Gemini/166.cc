#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRouting(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpClientServerHelper echoClientServer(9);
  echoClientServer.Install(nodes.Get(1), nodes.Get(3));

  Ptr<UdpServer> server = DynamicCast<UdpServer>(echoClientServer.GetServer());
  server->SetAttribute("MaxPackets", UintegerValue(1000000));

  Ptr<UdpClient> client = DynamicCast<UdpClient>(echoClientServer.GetClient());
  client->SetAttribute("MaxPackets", UintegerValue(1000000));
  client->SetAttribute("PacketSize", UintegerValue(1024));
  client->SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(3)));

  ApplicationContainer clientApps = client->Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  ApplicationContainer serverApps = server->Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  Simulator::Stop(Seconds(11.0));

  AnimationInterface anim("aodv_adhoc.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 20, 20);
  anim.SetConstantPosition(nodes.Get(2), 30, 30);
  anim.SetConstantPosition(nodes.Get(3), 70, 70);
  anim.SetConstantPosition(nodes.Get(4), 80, 80);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  phy.EnablePcapAll("aodv_adhoc");

  Simulator::Run();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	  std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	  std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
    }

  Simulator::Destroy();
  return 0;
}