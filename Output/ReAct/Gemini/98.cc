#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GlobalRoutingMixedNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices1, p2pDevices2, p2pDevices3;
  NetDeviceContainer csmaDevices;

  NodeContainer p2pNodes1, p2pNodes2, p2pNodes3, csmaNodes;

  p2pNodes1.Add (nodes.Get (0));
  p2pNodes1.Add (nodes.Get (1));

  p2pNodes2.Add (nodes.Get (1));
  p2pNodes2.Add (nodes.Get (2));

  p2pNodes3.Add (nodes.Get (2));
  p2pNodes3.Add (nodes.Get (3));

  csmaNodes.Add (nodes.Get (3));
  csmaNodes.Add (nodes.Get (4));
  csmaNodes.Add (nodes.Get (5));
  csmaNodes.Add (nodes.Get (6));

  p2pDevices1 = pointToPoint.Install (p2pNodes1);
  p2pDevices2 = pointToPoint.Install (p2pNodes2);
  p2pDevices3 = pointToPoint.Install (p2pNodes3);
  csmaDevices = csma.Install (csmaNodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign (p2pDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign (p2pDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3 = address.Assign (p2pDevices3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (6));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (60.0));

  UdpClientHelper echoClient (csmaInterfaces.GetAddress (3), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps1 = echoClient.Install (nodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (60.0));

  UdpClientHelper echoClient2 (csmaInterfaces.GetAddress (3), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (100000));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (0));
  clientApps2.Start (Seconds (11.0));
  clientApps2.Stop (Seconds (60.0));


  Simulator::Schedule (Seconds (20.0), &Ipv4Interface::SetDown,
                       p2pInterfaces2.GetInterface (0));

  Simulator::Schedule (Seconds (30.0), &Ipv4Interface::SetUp,
                       p2pInterfaces2.GetInterface (0));

  Simulator::Schedule (Seconds (40.0), &Ipv4Interface::SetDown,
                       p2pInterfaces3.GetInterface (0));

  Simulator::Schedule (Seconds (50.0), &Ipv4Interface::SetUp,
                       p2pInterfaces3.GetInterface (0));


  if (tracing)
    {
      pointToPoint.EnablePcapAll ("GlobalRoutingMixedNetwork");
      csma.EnablePcapAll ("GlobalRoutingMixedNetwork", false);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:    " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile("GlobalRoutingMixedNetwork.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}