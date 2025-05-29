#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospf-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LinkStateRoutingSimulation");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("OspfLsaGenerator", AttributeValue (BooleanValue (true)));
  LogComponent::SetAttribute ("OspfInterface", AttributeValue (BooleanValue (true)));
  LogComponent::SetAttribute ("OspfRouter", AttributeValue (BooleanValue (true)));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices13 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices34 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.SetRoutingHelper (ospf);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = ipv4.Assign (devices12);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces13 = ipv4.Assign (devices13);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = ipv4.Assign (devices23);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces34 = ipv4.Assign (devices34);

  ospf.Install (nodes.Get (0));
  ospf.Install (nodes.Get (1));
  ospf.Install (nodes.Get (2));
  ospf.Install (nodes.Get (3));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;  // Discard port (RFC 863)

  UdpClientHelper client (interfaces34.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  pointToPoint.EnablePcapAll ("link-state-routing");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}