#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospf-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfExample");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  std::string animFile = "ospf-example.xml";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enablePcap", "Enable or disable pcap tracing", enablePcap);
  cmd.AddValue ("animFile", "File Name for Animation Output", animFile);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("OspfInterface", LOG_LEVEL_ALL);
  LogComponent::SetFilter ("OspfNeighbor", LOG_LEVEL_ALL);
  LogComponent::SetFilter ("OspfLsa", LOG_LEVEL_ALL);
  LogComponent::SetFilter ("OspfRouter", LOG_LEVEL_ALL);
  LogComponent::SetFilter ("Ipv4L3Protocol", LOG_LEVEL_ALL);
  LogComponent::SetFilter ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::SetFilter ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer devices03 = pointToPoint.Install (nodes.Get (0), nodes.Get (3));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.AddProtocol (&ospf, 0);

  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces03 = address.Assign (devices03);

  ospf.Initialize ();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces23.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
    {
      pointToPoint.EnablePcapAll ("ospf-example");
    }

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
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 << " kbps\n";
    }

  monitor->SerializeToXmlFile("ospf-example.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}