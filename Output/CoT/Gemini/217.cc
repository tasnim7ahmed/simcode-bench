#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvManetSimulation");

int main (int argc, char *argv[])
{
  bool tracing = false;
  double simulationTime = 100;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable packet tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (5);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("0.0"),
                                 "Y", StringValue ("0.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=50]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  mobility.Install (nodes);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (4), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      phy.EnableAsciiAll (ascii.CreateFileStream ("aodv-manet.tr"));
      phy.EnablePcapAll ("aodv-manet");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("aodv-manet.xml");
  anim.SetMaxPktsPerTraceFile (500000);

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_UNCOND ("  Tx Packets: " << i->second.txPackets);
      NS_LOG_UNCOND ("  Rx Packets: " << i->second.rxPackets);
      NS_LOG_UNCOND ("  Lost Packets: " << i->second.lostPackets);
      NS_LOG_UNCOND ("  Packet Delivery Ratio: " << double(i->second.rxPackets)/double(i->second.txPackets));
      NS_LOG_UNCOND ("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 << " kbps");
    }

  monitor->SerializeToXmlFile("aodv-manet.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}