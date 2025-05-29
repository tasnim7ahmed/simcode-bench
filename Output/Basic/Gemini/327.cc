#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/random-waypoint-mobility-model.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetOlsrExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);
  LogComponent::Enable ("OlsrRoutingProtocol", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (6);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install (nodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (nodes);

  UdpClientServerHelper echoClientServer (9);
  echoClientServer.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClientServer.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientServer.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClientServer.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  ApplicationContainer serverApps = echoClientServer.Install (nodes.Get (5));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();

  return 0;
}