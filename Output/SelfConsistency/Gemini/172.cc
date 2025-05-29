#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocSimulation");

int
main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool enableNetAnim = false;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (10);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 500, 0, 500)));
  mobility.Install (nodes);

  // Install wireless devices
  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Install AODV routing
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  // Assign addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Create UDP traffic
  uint16_t port = 9; // Discard port (RFC 863)
  ApplicationContainer apps;

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    for (uint32_t j = 0; j < nodes.GetN (); ++j)
    {
      if (i == j)
        continue;

      Ptr<Node> source = nodes.Get (i);
      Ptr<Node> sink = nodes.Get (j);

      // Install receiver on the sink
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (j), port));
      ApplicationContainer sinkApp = sinkHelper.Install (sink);
      sinkApp.Start (Seconds (1.0));
      sinkApp.Stop (Seconds (30.0));

      // Install sender on the source
      OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (j), port));
      onoff.SetConstantRate (DataRate ("500kbps"));

      ApplicationContainer app = onoff.Install (source);
      app.Start (Seconds (5.0 + (double)i)); // Stagger start times
      app.Stop (Seconds (30.0));
      apps.Add (app);
    }
  }

  // Enable PCAP tracing
  if (enablePcap)
  {
    wifiPhy.EnablePcapAll ("aodv-adhoc");
  }

  // Enable NetAnim
  if (enableNetAnim)
  {
    AnimationInterface anim ("aodv-adhoc.xml");
  }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (30.0));
  Simulator::Run ();

  // Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalPdr = 0.0;
  double totalDelay = 0.0;
  int flowCount = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    if (t.sourceAddress != t.destinationAddress) {

        double pdr = 0;
        if (i->second.rxPackets > 0) {
            pdr = (double)i->second.rxPackets / (double)i->second.txPackets;
        }

        totalPdr += pdr;
        totalDelay += i->second.delaySum.GetSeconds();
        flowCount++;

        NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
        NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
        NS_LOG_UNCOND("  Packet Delivery Ratio: " << pdr);
        NS_LOG_UNCOND("  Delay Sum: " << i->second.delaySum);
    }
  }

  if (flowCount > 0) {
      NS_LOG_UNCOND("Average PDR: " << totalPdr / flowCount);
      NS_LOG_UNCOND("Average End-to-End Delay: " << totalDelay / flowCount);
  } else {
      NS_LOG_UNCOND("No flows found.");
  }


  Simulator::Destroy ();

  return 0;
}