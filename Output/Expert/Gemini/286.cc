#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(5);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
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
  Ssid ssid = Ssid("ns-3-adhoc");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Z", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
  mobility.Install(nodes);

  UdpServerHelper server(4000);
  ApplicationContainer serverApps = server.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpClientHelper client(interfaces.GetAddress(4), 4000);
  client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  client.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));

  phy.EnablePcapAll("manet");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
  }

  Simulator::Destroy();

  return 0;
}