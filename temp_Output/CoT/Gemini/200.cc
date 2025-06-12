#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  double simulationTime = 10.0;
  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  uint16_t port = 9;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Z", StringValue ("0.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  mobility.Install (nodes);

  UdpClientServerHelper cbr(interfaces.GetAddress(1), port);
  cbr.SetAttribute("PacketSize", UintegerValue(packetSize));
  cbr.SetAttribute("MaxPackets", UintegerValue(0xffffffff));

  ApplicationContainer clientApps = cbr.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime - 1));

  UdpClientServerHelper cbr2(interfaces.GetAddress(3), port);
  cbr2.SetAttribute("PacketSize", UintegerValue(packetSize));
  cbr2.SetAttribute("MaxPackets", UintegerValue(0xffffffff));

  ApplicationContainer clientApps2 = cbr2.Install(nodes.Get(2));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime - 2));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (enablePcap)
  {
    phy.EnablePcapAll ("adhoc-aodv");
  }

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("adhoc-aodv.xml");
  anim.SetMaxPktsPerTraceFile(500000);

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets/(double)i->second.txPackets) << "\n";
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
      std::cout << "  End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << "\n";
    }

  Simulator::Destroy ();

  return 0;
}