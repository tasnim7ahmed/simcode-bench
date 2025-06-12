#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ospf-example");

int main (int argc, char *argv[])
{
  bool tracing = true;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer routers;
  routers.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer r0r1Devs = pointToPoint.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer r0r2Devs = pointToPoint.Install (routers.Get (0), routers.Get (2));
  NetDeviceContainer r1r3Devs = pointToPoint.Install (routers.Get (1), routers.Get (3));
  NetDeviceContainer r2r3Devs = pointToPoint.Install (routers.Get (2), routers.Get (3));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.Install (routers);
  ospf.Install (routers.Get (0));
  ospf.Install (routers.Get (1));
  ospf.Install (routers.Get (2));
  ospf.Install (routers.Get (3));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r0r1Ifaces = ipv4.Assign (r0r1Devs);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer r0r2Ifaces = ipv4.Assign (r0r2Devs);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r3Ifaces = ipv4.Assign (r1r3Devs);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer r2r3Ifaces = ipv4.Assign (r2r3Devs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (routers.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1.0));

  UdpEchoClientHelper echoClient (r0r3Ifaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (routers.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2.0));

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("ospf-example");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("ospf-example.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}