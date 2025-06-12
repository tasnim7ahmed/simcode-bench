#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetwork");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.AddValue ("simulationTime", "Simulation Time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLevel (NS_LOG_COMPONENT, LOG_LEVEL_INFO);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer sensorNodes;
  sensorNodes.Create (5);

  // Set up the LR-WPAN PHY and MAC layers
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer sinkDevice = lrWpanHelper.Install (sinkNode);
  NetDeviceContainer sensorDevices = lrWpanHelper.Install (sensorNodes);

  // Set up the 6LoWPAN network layer
  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanSinkDevice = sixLowPanHelper.Install (sinkDevice);
  NetDeviceContainer sixLowPanSensorDevices = sixLowPanHelper.Install (sensorDevices);

  // Install the Internet stack
  InternetStackHelper internet;
  internet.Install (sinkNode);
  internet.Install (sensorNodes);

  // Assign IP addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer sinkInterface = ipv6.Assign (sixLowPanSinkDevice);
  Ipv6InterfaceContainer sensorInterfaces = ipv6.Assign (sixLowPanSensorDevices);

  // Set up the positions of the nodes
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sinkNode);
  Ptr<Node> sink = sinkNode.Get (0);
  Ptr<ConstantPositionMobilityModel> sinkPos = sink->GetObject<ConstantPositionMobilityModel> ();
  sinkPos->SetPosition (Vector (10.0, 10.0, 0.0));

  // Set up the UDP server on the sink node
  UdpServerHelper server (9);
  ApplicationContainer sinkApp = server.Install (sinkNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (simulationTime));

  // Set up the UDP client on the sensor nodes
  UdpClientHelper client (sinkInterface.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (10000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  client.SetAttribute ("PacketSize", UintegerValue (128));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      clientApps.Add (client.Install (sensorNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Create FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable PCAP Tracing
  if (enablePcap)
    {
      lrWpanHelper.EnablePcapAll ("sensorNetwork");
    }

  // Enable NetAnim
  AnimationInterface anim ("sensor-network.xml");
  anim.SetConstantPosition (sinkNode.Get (0), 10, 10);
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
  {
    Ptr<Node> node = sensorNodes.Get (i);
    Ptr<ConstantPositionMobilityModel> pos = node->GetObject<ConstantPositionMobilityModel> ();
    Vector3D position = pos->GetPosition ();
    anim.SetConstantPosition (node, position.x, position.y);
  }

  // Run the simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print Flow Monitor statistics
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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 /1024  << " Mbps\n";
      std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s\n";
    }

  Simulator::Destroy ();

  return 0;
}