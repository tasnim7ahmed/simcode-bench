#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/6lowpan-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSNSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable("WSNSimulation", LOG_LEVEL_INFO);
  bool verbose = true;
  if (verbose)
    {
      LogComponentEnable ("UdpClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (6);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (6),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t sinkPort = 9;
  UdpServerHelper server (sinkPort);
  ApplicationContainer sinkApps = server.Install (nodes.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (100.0));

  UdpClientHelper client (interfaces.GetAddress (0), sinkPort);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN (); ++i)
    {
      clientApps.Add (client.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (100.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  AnimationInterface anim ("wsn_simulation.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      anim.UpdateNodeDescription (nodes.Get(i), "Node-" + std::to_string(i));
    }
  anim.UpdateNodeColor (nodes.Get(0), 255, 0, 0); // Sink node in red

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::cout << "\nPerformance Metrics:\n";
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  PDR: " << ((double)i->second.rxPackets / (double)i->second.txPackets) * 100 << "%\n";
      std::cout << "  Avg Delay: " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s\n";
      std::cout << "  Throughput: " << (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 << " Kbps\n";
    }

  Simulator::Destroy ();
  return 0;
}