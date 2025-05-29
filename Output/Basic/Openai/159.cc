#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer starCentral;
  starCentral.Create (1); // Node 0

  NodeContainer starPeripherals;
  starPeripherals.Create (3); // Nodes 1,2,3

  NodeContainer busNodes;
  busNodes.Create (3); // Nodes 4,5,6

  Ptr<Node> n0 = starCentral.Get (0);
  Ptr<Node> n1 = starPeripherals.Get (0);
  Ptr<Node> n2 = starPeripherals.Get (1);
  Ptr<Node> n3 = starPeripherals.Get (2);
  Ptr<Node> n4 = busNodes.Get (0);
  Ptr<Node> n5 = busNodes.Get (1);
  Ptr<Node> n6 = busNodes.Get (2);

  // Star: Node 0 <-> Node 1,2,3
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0n1 = p2p.Install (n0, n1);
  NetDeviceContainer d0n2 = p2p.Install (n0, n2);
  NetDeviceContainer d0n3 = p2p.Install (n0, n3);

  // Bus: 4-5-6 as 4<->5, 5<->6
  NetDeviceContainer d4n5 = p2p.Install (n4, n5);
  NetDeviceContainer d5n6 = p2p.Install (n5, n6);

  // Hybrid Connectivity: connect Node 3 (star peripheral) with Node 4 (bus start node)
  // To tie star and bus together, either directly connect or via router
  // Let's connect Node 3 and Node 4 directly
  NetDeviceContainer d3n4 = p2p.Install (n3, n4);

  // Install internet stack on all nodes
  NodeContainer allNodes;
  allNodes.Add (starCentral);
  allNodes.Add (starPeripherals);
  allNodes.Add (busNodes);

  InternetStackHelper internet;
  internet.Install (allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  // n0-n1
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0n1);

  // n0-n2
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign (d0n2);

  // n0-n3
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i3 = address.Assign (d0n3);

  // n3-n4 (joint between star and bus)
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = address.Assign (d3n4);

  // n4-n5
  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = address.Assign (d4n5);

  // n5-n6
  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i5i6 = address.Assign (d5n6);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo Server on Node 0
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n0);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  // UDP Echo Client on Node 4
  UdpEchoClientHelper echoClient1 (i0i1.GetAddress (0), echoPort); // target: node 0's address
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps1 = echoClient1.Install (n4);
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (20.0));

  // UDP Echo Client on Node 5
  UdpEchoClientHelper echoClient2 (i0i1.GetAddress (0), echoPort); // target: node 0's address
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (n5);
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (20.0));

  // FlowMonitor
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
      if (iter->second.rxPackets > 0)
        {
          std::cout << "  Mean Delay: " << (iter->second.delaySum.GetSeconds () / iter->second.rxPackets) << " s\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}