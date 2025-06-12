#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  double simulationTime = 20.0;

  // Node containers
  NodeContainer meshNodes;
  meshNodes.Create (4);

  NodeContainer treeNodes;
  treeNodes.Create (4);

  // Install mobility for visualization (static positions, animating traffic)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> meshPos = CreateObject<ListPositionAllocator> ();
  meshPos->Add (Vector (20.0, 80.0, 0.0));
  meshPos->Add (Vector (40.0, 100.0, 0.0));
  meshPos->Add (Vector (60.0, 80.0, 0.0));
  meshPos->Add (Vector (40.0, 60.0, 0.0));
  mobility.SetPositionAllocator (meshPos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  Ptr<ListPositionAllocator> treePos = CreateObject<ListPositionAllocator> ();
  treePos->Add (Vector (120.0, 80.0, 0.0)); // root
  treePos->Add (Vector (140.0, 100.0, 0.0)); // child 1
  treePos->Add (Vector (140.0, 80.0, 0.0));  // child 2
  treePos->Add (Vector (140.0, 60.0, 0.0));  // child 3
  mobility.SetPositionAllocator (treePos);
  mobility.Install (treeNodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (meshNodes);
  stack.Install (treeNodes);

  // -- Mesh Topology: Use mesh helper (IEEE802.11s) --
  MeshHelper meshHelper = MeshHelper::Default ();
  meshHelper.SetStackInstaller ("ns3::Dot11sStack");
  meshHelper.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  meshHelper.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  meshHelper.SetNumberOfInterfaces (1);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  NetDeviceContainer meshDevices = meshHelper.Install (wifiPhy, meshNodes);

  // -- Tree Topology: Use point-to-point links --
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NodeContainer t0t1 (treeNodes.Get (0), treeNodes.Get (1));
  NodeContainer t0t2 (treeNodes.Get (0), treeNodes.Get (2));
  NodeContainer t0t3 (treeNodes.Get (0), treeNodes.Get (3));

  NetDeviceContainer d0d1 = p2pHelper.Install (t0t1);
  NetDeviceContainer d0d2 = p2pHelper.Install (t0t2);
  NetDeviceContainer d0d3 = p2pHelper.Install (t0t3);

  // -- Assign IP addresses --
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = ipv4.Assign (meshDevices);

  ipv4.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);

  ipv4.SetBase ("10.2.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign (d0d2);

  ipv4.SetBase ("10.2.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i3 = ipv4.Assign (d0d3);

  // -- Routing --
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // -- UDP Applications --
  // Mesh: node 0 sends to node 2
  uint16_t meshPort = 9000;
  UdpServerHelper meshServer (meshPort);
  ApplicationContainer meshServerApps = meshServer.Install (meshNodes.Get (2));
  meshServerApps.Start (Seconds (1.0));
  meshServerApps.Stop (Seconds (simulationTime));

  UdpClientHelper meshClient (meshInterfaces.GetAddress (2), meshPort);
  meshClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  meshClient.SetAttribute ("Interval", TimeValue (MilliSeconds (20)));
  meshClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer meshClientApps = meshClient.Install (meshNodes.Get (0));
  meshClientApps.Start (Seconds (2.0));
  meshClientApps.Stop (Seconds (simulationTime));

  // Tree: node 1 sends to node 3
  uint16_t treePort = 9001;
  UdpServerHelper treeServer (treePort);
  ApplicationContainer treeServerApps = treeServer.Install (treeNodes.Get (3));
  treeServerApps.Start (Seconds (1.0));
  treeServerApps.Stop (Seconds (simulationTime));

  Ipv4Address treeDstAddr = i0i3.GetAddress (1); // node 3 address
  UdpClientHelper treeClient (treeDstAddr, treePort);
  treeClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  treeClient.SetAttribute ("Interval", TimeValue (MilliSeconds (20)));
  treeClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer treeClientApps = treeClient.Install (treeNodes.Get (1));
  treeClientApps.Start (Seconds (2.0));
  treeClientApps.Stop (Seconds (simulationTime));

  // -- FlowMonitor --
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();

  // -- NetAnim --
  AnimationInterface anim ("mesh-tree-comparison.xml");
  // Mesh node labels
  anim.UpdateNodeDescription (meshNodes.Get (0), "M0");
  anim.UpdateNodeDescription (meshNodes.Get (1), "M1");
  anim.UpdateNodeDescription (meshNodes.Get (2), "M2");
  anim.UpdateNodeDescription (meshNodes.Get (3), "M3");

  // Tree node labels
  anim.UpdateNodeDescription (treeNodes.Get (0), "T0");
  anim.UpdateNodeDescription (treeNodes.Get (1), "T1");
  anim.UpdateNodeDescription (treeNodes.Get (2), "T2");
  anim.UpdateNodeDescription (treeNodes.Get (3), "T3");

  // Animate UDP traffic as packet bursts
  anim.EnablePacketMetadata (true);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // -- FlowMonitor results --
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());

  double meshTxBytes = 0, meshRxBytes = 0, meshDelaySum = 0, meshPackets = 0;
  double treeTxBytes = 0, treeRxBytes = 0, treeDelaySum = 0, treePackets = 0;
  double meshFirstTx = 1e9, meshLastRx = 0, treeFirstTx = 1e9, treeLastRx = 0;

  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);

      // Mesh UDP flow
      if (t.sourceAddress == meshInterfaces.GetAddress (0) &&
          t.destinationAddress == meshInterfaces.GetAddress (2))
        {
          meshTxBytes += it->second.txBytes;
          meshRxBytes += it->second.rxBytes;
          meshDelaySum += it->second.delaySum.GetSeconds ();
          meshPackets += it->second.rxPackets;
          if (it->second.timeFirstTxPacket.GetSeconds () < meshFirstTx) meshFirstTx = it->second.timeFirstTxPacket.GetSeconds ();
          if (it->second.timeLastRxPacket.GetSeconds () > meshLastRx) meshLastRx = it->second.timeLastRxPacket.GetSeconds ();
        }
      // Tree UDP flow
      else if (t.sourceAddress == i0i1.GetAddress (1) && t.destinationAddress == i0i3.GetAddress (1))
        {
          treeTxBytes += it->second.txBytes;
          treeRxBytes += it->second.rxBytes;
          treeDelaySum += it->second.delaySum.GetSeconds ();
          treePackets += it->second.rxPackets;
          if (it->second.timeFirstTxPacket.GetSeconds () < treeFirstTx) treeFirstTx = it->second.timeFirstTxPacket.GetSeconds ();
          if (it->second.timeLastRxPacket.GetSeconds () > treeLastRx) treeLastRx = it->second.timeLastRxPacket.GetSeconds ();
        }
    }

  double meshThroughput = (meshRxBytes * 8.0) / (1000000 * (meshLastRx - meshFirstTx)); // Mbps
  double meshLatency = meshPackets > 0 ? (meshDelaySum / meshPackets) : 0;
  double meshPDR = meshTxBytes > 0 ? (meshRxBytes / meshTxBytes) * 100 : 0;

  double treeThroughput = (treeRxBytes * 8.0) / (1000000 * (treeLastRx - treeFirstTx)); // Mbps
  double treeLatency = treePackets > 0 ? (treeDelaySum / treePackets) : 0;
  double treePDR = treeTxBytes > 0 ? (treeRxBytes / treeTxBytes) * 100 : 0;

  std::cout << "===== Mesh vs Tree Performance Comparison =====" << std::endl;
  std::cout << "  Mesh Topology:" << std::endl;
  std::cout << "    Throughput          : " << meshThroughput << " Mbps" << std::endl;
  std::cout << "    Average Latency     : " << meshLatency * 1000 << " ms" << std::endl;
  std::cout << "    Packet Delivery Rate: " << meshPDR << " %" << std::endl << std::endl;

  std::cout << "  Tree Topology:" << std::endl;
  std::cout << "    Throughput          : " << treeThroughput << " Mbps" << std::endl;
  std::cout << "    Average Latency     : " << treeLatency * 1000 << " ms" << std::endl;
  std::cout << "    Packet Delivery Rate: " << treePDR << " %" << std::endl;

  Simulator::Destroy ();
  return 0;
}