#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  LogComponent::SetLogLevel (LOG_LEVEL_ALL);
  LogComponent::SetEnableAllLogCategories (true);

  NodeContainer nodes;
  nodes.Create (10);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"),
                             "Bounds", StringValue ("500x500"));
  mobility.Install (nodes);

  // WIFI configuration
  WifiHelper wifi;
  WifiMacHelper mac;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Create ();
  phy.SetChannel (channel.Create ());

  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  mac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // AODV routing
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP server (node 0)
  UdpEchoServerHelper echoServer (4000);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  // UDP client (node 1)
  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 4000);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (20));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (20.0));

  // PCAP tracing
  if (enablePcap)
    {
      phy.EnablePcapAll ("manet-aodv");
    }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (20.0));

  AnimationInterface anim ("manet-aodv.xml");

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";

    }


  Simulator::Destroy ();

  return 0;
}