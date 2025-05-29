#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer csmaNodes;
  csmaNodes.Create (5);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (2)));

  NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

  InternetStackHelper stack;
  stack.Install (csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (csmaDevices);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (4), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (csmaNodes.Get (4));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute ("SendSize", UintegerValue (1400));

  ApplicationContainer sourceApps;
  for (int i = 0; i < 4; ++i)
  {
    sourceApps.Add (source.Install (csmaNodes.Get (i)));
  }

  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (9.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  csma.EnablePcap ("csma", csmaDevices, false);

  AnimationInterface anim ("csma-animation.xml");
  anim.SetConstantPosition (csmaNodes.Get (0), 10, 10);
  anim.SetConstantPosition (csmaNodes.Get (1), 30, 10);
  anim.SetConstantPosition (csmaNodes.Get (2), 50, 10);
  anim.SetConstantPosition (csmaNodes.Get (3), 70, 10);
  anim.SetConstantPosition (csmaNodes.Get (4), 90, 10);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (11.0));

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
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
  }

  monitor->SerializeToXmlFile("csma.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}