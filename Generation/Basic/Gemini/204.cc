#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <iomanip>

int main (int argc, char *argv[])
{
  uint32_t numNodes = 6;
  double simTime = 20.0;
  uint32_t packetSize = 1024;
  double sendInterval = 1.0;
  uint32_t maxPackets = 15;

  ns3::NodeContainer nodes;
  nodes.Create (numNodes);

  ns3::MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set positions for a 3x2 grid
  // Node 0: Base Station (0,0,0)
  // Node 1: Sensor 1 (10,0,0)
  // Node 2: Sensor 2 (20,0,0)
  // Node 3: Sensor 3 (0,10,0)
  // Node 4: Sensor 4 (10,10,0)
  // Node 5: Sensor 5 (20,10,0)
  nodes.Get (0)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (0.0, 0.0, 0.0));
  nodes.Get (1)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (10.0, 0.0, 0.0));
  nodes.Get (2)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (20.0, 0.0, 0.0));
  nodes.Get (3)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (0.0, 10.0, 0.0));
  nodes.Get (4)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (10.0, 10.0, 0.0));
  nodes.Get (5)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (20.0, 10.0, 0.0));

  ns3::CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", ns3::StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", ns3::TimeValue (ns3::NanoSeconds (65600)));

  ns3::NetDeviceContainer devices = csma.Install (nodes);

  ns3::InternetStackHelper internet;
  internet.Install (nodes);

  ns3::Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Base Station (Server) - Node 0
  uint16_t port = 9;
  ns3::PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                    ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), port));
  ns3::ApplicationContainer serverApp = sinkHelper.Install (nodes.Get (0));
  serverApp.Start (ns3::Seconds (1.0));
  serverApp.Stop (ns3::Seconds (simTime - 1.0));

  // Sensor Nodes (Clients) - Nodes 1 to 5
  ns3::Ipv4Address baseStationIp = interfaces.GetAddress (0);
  ns3::UdpClientHelper clientHelper (baseStationIp, port);
  clientHelper.SetAttribute ("MaxPackets", ns3::UintegerValue (maxPackets));
  clientHelper.SetAttribute ("Interval", ns3::TimeValue (ns3::Seconds (sendInterval)));
  clientHelper.SetAttribute ("PacketSize", ns3::UintegerValue (packetSize));

  ns3::ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i)
  {
    clientApps.Add (clientHelper.Install (nodes.Get (i)));
  }
  clientApps.Start (ns3::Seconds (2.0));
  clientApps.Stop (ns3::Seconds (simTime - 0.5));

  ns3::AnimationInterface anim ("wsn-csma-grid.xml");
  anim.SetMaxPktsPerTraceFile (5000);
  anim.SetNodeColor (nodes.Get (0), 0, 0, 255); // Blue for Base Station
  for (uint32_t i = 1; i < numNodes; ++i)
  {
    anim.SetNodeColor (nodes.Get (i), 255, 0, 0); // Red for Sensor Nodes
  }
  anim.SetNodeSize (0, 2.0, 2.0);
  for (uint32_t i = 1; i < numNodes; ++i)
  {
    anim.SetNodeSize (i, 1.0, 1.0);
  }
  anim.EnablePacketMetadata (true);

  ns3::FlowMonitorHelper flowmon;
  ns3::Ptr<ns3::FlowMonitor> monitor = flowmon.InstallAll ();

  ns3::Simulator::Stop (ns3::Seconds (simTime));
  ns3::Simulator::Run ();

  monitor->CheckForFlowChanges ();
  ns3::Ptr<ns3::Ipv4FlowClassifier> classifier = ns3::DynamicCast<ns3::Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalTxPackets = 0;
  double totalRxPackets = 0;
  double totalDelaySeconds = 0;
  uint32_t receivedPacketCount = 0;

  for (std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    ns3::Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    if (t.sourceAddress != baseStationIp && t.destinationAddress == baseStationIp)
    {
      totalTxPackets += i->second.txPackets;
      totalRxPackets += i->second.rxPackets;
      totalDelaySeconds += i->second.delaySum.GetSeconds ();
      receivedPacketCount += i->second.rxPackets;
    }
  }

  double pdr = 0.0;
  if (totalTxPackets > 0)
  {
    pdr = (totalRxPackets / totalTxPackets) * 100.0;
  }

  double avgEndToEndDelay = 0.0;
  if (receivedPacketCount > 0)
  {
    avgEndToEndDelay = totalDelaySeconds / receivedPacketCount;
  }

  std::cout << "\n--- Simulation Metrics ---" << std::endl;
  std::cout << "Total Packets Sent by Sensor Nodes: " << totalTxPackets << std::endl;
  std::cout << "Total Packets Received by Base Station: " << totalRxPackets << std::endl;
  std::cout << "Packet Delivery Ratio (PDR): " << std::fixed << std::setprecision(2) << pdr << "%" << std::endl;
  std::cout << "Average End-to-End Delay: " << std::fixed << std::setprecision(6) << avgEndToEndDelay << " seconds" << std::endl;
  std::cout << "--------------------------" << std::endl;

  ns3::Simulator::Destroy ();

  return 0;
}