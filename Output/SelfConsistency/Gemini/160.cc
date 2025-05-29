#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mesh-module.h"
#include "ns3/tree-helper.h"
#include "ns3/mobility-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshVsTree");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  // Mesh Network
  NodeContainer meshNodes;
  meshNodes.Create (4);

  PointToPointHelper p2pMesh;
  p2pMesh.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2pMesh.SetChannelAttribute ("Delay", StringValue ("2ms"));

  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::TcpL4Protocol"); // Explicitly set the stack
  mesh.SetMacType ("ns3::AdhocWifiMac");

  Ipv4AddressHelper addressMesh;
  addressMesh.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = addressMesh.Assign (mesh.Install (meshNodes));

  // Tree Network
  NodeContainer treeNodes;
  treeNodes.Create (4);

  PointToPointHelper p2pTree;
  p2pTree.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2pTree.SetChannelAttribute ("Delay", StringValue ("2ms"));

  TreeHelper tree (4); // Create a tree topology with 4 nodes
  tree.SetPointToPointHelper (p2pTree);
  NetDeviceContainer treeDevices = tree.Install (treeNodes);

  InternetStackHelper stack;
  stack.Install (treeNodes);

  Ipv4AddressHelper addressTree;
  addressTree.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces = addressTree.Assign (treeDevices);


  // Mobility for Visualization
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);
  mobility.Install (treeNodes);

  // UDP Traffic for Mesh
  UdpEchoServerHelper echoServerMesh (9);
  ApplicationContainer serverAppsMesh = echoServerMesh.Install (meshNodes.Get (3));
  serverAppsMesh.Start (Seconds (1.0));
  serverAppsMesh.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClientMesh (meshInterfaces.GetAddress (3, 0), 9);
  echoClientMesh.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClientMesh.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClientMesh.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppsMesh = echoClientMesh.Install (meshNodes.Get (0));
  clientAppsMesh.Start (Seconds (2.0));
  clientAppsMesh.Stop (Seconds (20.0));

  // UDP Traffic for Tree
  UdpEchoServerHelper echoServerTree (9);
  ApplicationContainer serverAppsTree = echoServerTree.Install (treeNodes.Get (3));
  serverAppsTree.Start (Seconds (1.0));
  serverAppsTree.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClientTree (treeInterfaces.GetAddress (3, 0), 9);
  echoClientTree.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClientTree.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClientTree.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppsTree = echoClientTree.Install (treeNodes.Get (0));
  clientAppsTree.Start (Seconds (2.0));
  clientAppsTree.Stop (Seconds (20.0));


  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Animation
  AnimationInterface anim ("mesh-vs-tree.xml");
  anim.SetConstantPosition (meshNodes.Get (0), 10, 10);
  anim.SetConstantPosition (meshNodes.Get (1), 20, 10);
  anim.SetConstantPosition (meshNodes.Get (2), 10, 20);
  anim.SetConstantPosition (meshNodes.Get (3), 20, 20);

  anim.SetConstantPosition (treeNodes.Get (0), 40, 10);
  anim.SetConstantPosition (treeNodes.Get (1), 50, 10);
  anim.SetConstantPosition (treeNodes.Get (2), 40, 20);
  anim.SetConstantPosition (treeNodes.Get (3), 50, 20);

  // Run Simulation
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  // Print Flow Monitor Statistics
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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
      std::cout << "  End to End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << "\n";
    }

  Simulator::Destroy ();
  return 0;
}