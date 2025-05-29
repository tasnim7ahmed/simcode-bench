#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvManetExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (10);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install (nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  UdpClientServerHelper csh (9, "10.1.1.1");
  csh.SetAttribute ("MaxPackets", UintegerValue (1000000));
  csh.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  csh.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
  rand->SetAttribute ("Min", DoubleValue (0));
  rand->SetAttribute ("Max", DoubleValue (nodes.GetN () - 1));

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ipv4Address target = interfaces.GetAddress (static_cast<uint32_t> (rand->GetValue ()));
      csh.SetAttribute ("RemoteAddress", AddressValue (InetSocketAddress (target, 9)));
      clientApps.Add (csh.Install (node));
    }

  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (20.0));

  Simulator::Stop (Seconds (20.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }
  monitor->SerializeToXmlFile("aodv-manet.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}