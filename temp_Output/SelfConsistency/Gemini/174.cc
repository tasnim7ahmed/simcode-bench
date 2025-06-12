#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridTopology");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  //
  // Create the hybrid topology
  //

  // 1. Ring Topology
  NodeContainer ringNodes;
  ringNodes.Create (5);

  PointToPointHelper ringP2P;
  ringP2P.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  ringP2P.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer ringDevices;
  for (uint32_t i = 0; i < ringNodes.GetN (); ++i)
    {
      NetDeviceContainer link;
      link = ringP2P.Install (ringNodes.Get (i), ringNodes.Get ((i + 1) % ringNodes.GetN ()));
      ringDevices.Add (link.Get (0));
      ringDevices.Add (link.Get (1));
    }

  // 2. Mesh Topology
  NodeContainer meshNodes;
  meshNodes.Create (3);

  PointToPointHelper meshP2P;
  meshP2P.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  meshP2P.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer meshDevices;
  for (uint32_t i = 0; i < meshNodes.GetN (); ++i)
    {
      for (uint32_t j = i + 1; j < meshNodes.GetN (); ++j)
        {
          NetDeviceContainer link;
          link = meshP2P.Install (meshNodes.Get (i), meshNodes.Get (j));
          meshDevices.Add (link.Get (0));
          meshDevices.Add (link.Get (1));
        }
    }

  // 3. Bus Topology (represented as a chain)
  NodeContainer busNodes;
  busNodes.Create (4);

  PointToPointHelper busP2P;
  busP2P.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  busP2P.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer busDevices;
  for (uint32_t i = 0; i < busNodes.GetN () - 1; ++i)
    {
      NetDeviceContainer link;
      link = busP2P.Install (busNodes.Get (i), busNodes.Get (i + 1));
      busDevices.Add (link.Get (0));
      busDevices.Add (link.Get (1));
    }

  // 4. Line Topology (already covered by bus)

  // 5. Star Topology
  NodeContainer starNodes;
  starNodes.Create (4); // 1 central + 3 spokes

  PointToPointHelper starP2P;
  starP2P.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  starP2P.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer starDevices;
  for (uint32_t i = 1; i < starNodes.GetN (); ++i)
    {
      NetDeviceContainer link;
      link = starP2P.Install (starNodes.Get (0), starNodes.Get (i));
      starDevices.Add (link.Get (0));
      starDevices.Add (link.Get (1));
    }

  // 6. Tree Topology
  NodeContainer treeNodes;
  treeNodes.Create (7); // Root + 2 children + 4 grandchildren

  PointToPointHelper treeP2P;
  treeP2P.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  treeP2P.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer treeDevices;
  // Level 1
  NetDeviceContainer link01 = treeP2P.Install (treeNodes.Get (0), treeNodes.Get (1));
  treeDevices.Add (link01.Get (0));
  treeDevices.Add (link01.Get (1));
  NetDeviceContainer link02 = treeP2P.Install (treeNodes.Get (0), treeNodes.Get (2));
  treeDevices.Add (link02.Get (0));
  treeDevices.Add (link02.Get (1));
  // Level 2
  NetDeviceContainer link13 = treeP2P.Install (treeNodes.Get (1), treeNodes.Get (3));
  treeDevices.Add (link13.Get (0));
  treeDevices.Add (link13.Get (1));
  NetDeviceContainer link14 = treeP2P.Install (treeNodes.Get (1), treeNodes.Get (4));
  treeDevices.Add (link14.Get (0));
  treeDevices.Add (link14.Get (1));
  NetDeviceContainer link25 = treeP2P.Install (treeNodes.Get (2), treeNodes.Get (5));
  treeDevices.Add (link25.Get (0));
  treeDevices.Add (link25.Get (1));
  NetDeviceContainer link26 = treeP2P.Install (treeNodes.Get (2), treeNodes.Get (6));
  treeDevices.Add (link26.Get (0));
  treeDevices.Add (link26.Get (1));

  // Connect the topologies
  PointToPointHelper connectP2P;
  connectP2P.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  connectP2P.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer connect1 = connectP2P.Install (ringNodes.Get (0), meshNodes.Get (0));
  NetDeviceContainer connect2 = connectP2P.Install (busNodes.Get (0), starNodes.Get (0));
  NetDeviceContainer connect3 = connectP2P.Install (starNodes.Get (1), treeNodes.Get (0));

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (ringNodes);
  stack.Install (meshNodes);
  stack.Install (busNodes);
  stack.Install (starNodes);
  stack.Install (treeNodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces = address.Assign (ringDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = address.Assign (meshDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces = address.Assign (busDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces = address.Assign (starDevices);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces = address.Assign (treeDevices);

  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer connectInterface1 = address.Assign (connect1);

  address.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer connectInterface2 = address.Assign (connect2);

  address.SetBase ("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer connectInterface3 = address.Assign (connect3);

  // Install and run OSPF
  OlsrHelper olsr;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create a UDP echo client and server
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (treeNodes.Get (6));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (treeInterfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (ringNodes.Get (4));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable Tracing
  PointToPointHelper::EnablePcapAll ("hybrid");

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable NetAnim
  AnimationInterface anim ("hybrid-topology.xml");

  // Start Simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  // Print FlowMonitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}