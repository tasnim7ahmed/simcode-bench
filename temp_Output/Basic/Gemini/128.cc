#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LinkStateRouting");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool enableNetAnim = true;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("LinkStateRouting", LOG_LEVEL_INFO);
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    }

  // Create nodes
  NodeContainer routers;
  routers.Create (4);

  // Install Internet stack
  InternetStackHelper internet;
  OlsrHelper olsr;
  internet.SetRoutingHelper (olsr);
  internet.Install (routers);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer routerDevices[6];
  routerDevices[0] = p2p.Install (routers.Get (0), routers.Get (1));
  routerDevices[1] = p2p.Install (routers.Get (0), routers.Get (2));
  routerDevices[2] = p2p.Install (routers.Get (1), routers.Get (3));
  routerDevices[3] = p2p.Install (routers.Get (2), routers.Get (3));
  routerDevices[4] = p2p.Install (routers.Get (1), routers.Get (2));
  routerDevices[5] = p2p.Install (routers.Get (0), routers.Get (3));

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces[6];
  routerInterfaces[0] = ipv4.Assign (routerDevices[0]);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  routerInterfaces[1] = ipv4.Assign (routerDevices[1]);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  routerInterfaces[2] = ipv4.Assign (routerDevices[2]);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  routerInterfaces[3] = ipv4.Assign (routerDevices[3]);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  routerInterfaces[4] = ipv4.Assign (routerDevices[4]);

  ipv4.SetBase ("10.1.6.0", "255.255.255.0");
  routerInterfaces[5] = ipv4.Assign (routerDevices[5]);

  // Create traffic source and sink
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (routerInterfaces[3].GetAddress (1), port)));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  ApplicationContainer apps = onoff.Install (routers.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (routers.Get (3));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Enable global routing - Not required when using OLSR or other dynamic routing protocols.
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable pcap tracing
  if (enablePcap)
    {
      p2p.EnablePcapAll ("linkstate");
    }

  // Enable NetAnim
  if (enableNetAnim)
    {
      AnimationInterface anim ("linkstate.xml");
      anim.SetConstantPosition (routers.Get (0), 10, 10);
      anim.SetConstantPosition (routers.Get (1), 30, 10);
      anim.SetConstantPosition (routers.Get (2), 10, 30);
      anim.SetConstantPosition (routers.Get (3), 30, 30);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

    monitor->SerializeToXmlFile("linkstate.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}