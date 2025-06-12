#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/sixlowpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetworkStar802154Example");

int
main (int argc, char *argv[])
{
  uint32_t numSensors = 5;
  double simTime = 20.0;
  std::string dataRate = "5kbps";
  double packetInterval = 2.0; // seconds
  uint32_t packetSize = 40; // bytes

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.Parse (argc, argv);

  NodeContainer sinkNode;
  sinkNode.Create (1);
  NodeContainer sensorNodes;
  sensorNodes.Create (numSensors);

  NodeContainer allNodes;
  allNodes.Add (sinkNode);
  allNodes.Add (sensorNodes);

  // Mobility: place the sink at the center, sensors around it in a circle
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  double radius = 20.0;
  double centerX = 50.0;
  double centerY = 50.0;
  positionAlloc->Add (Vector(centerX, centerY, 0.0)); // Sink at center
  for (uint32_t i = 0; i < numSensors; ++i)
    {
      double angle = i * (2 * M_PI / numSensors);
      double x = centerX + radius * std::cos(angle);
      double y = centerY + radius * std::sin(angle);
      positionAlloc->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // 802.15.4 + 6LoWPAN
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrwpanDevs = lrWpanHelper.Install (allNodes);

  lrWpanHelper.AssociateToPan (lrwpanDevs, 0);
  SixLowPanHelper sixlowpan;
  NetDeviceContainer sixlowpanDevices = sixlowpan.Install (lrwpanDevs);

  // Internet stack with IPv6
  InternetStackHelper internetv6;
  internetv6.Install (allNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixlowpanDevices);
  interfaces.SetForwarding (0, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  // UDP server: on sink node (node 0)
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (sinkNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // UDP clients at sensor nodes
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numSensors; ++i)
    {
      UdpClientHelper client (interfaces.GetAddress (0,1), port);
      client.SetAttribute ("MaxPackets", UintegerValue (uint32_t(simTime/packetInterval) + 1));
      client.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));
      clientApps.Add (client.Install (sensorNodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simTime));

  // Enable packet capturing (optional)
  lrWpanHelper.EnablePcapAll (std::string("star-802154"));

  // Animation
  AnimationInterface anim ("star-sensor-network.xml");
  anim.SetConstantPosition (sinkNode.Get(0), centerX, centerY, 0);
  for (uint32_t i=0; i<numSensors; ++i)
    {
      Ptr<Node> node = sensorNodes.Get(i);
      Vector pos = node->GetObject<MobilityModel>()->GetPosition ();
      anim.SetConstantPosition (node, pos.x, pos.y, 0);
    }

  // FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Results: throughput, delivery, latency
  monitor->CheckForLostPackets ();
  Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier> (flowmonHelper.GetClassifier6 ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << "Flow statistics:" << std::endl;
  for (auto & flow : stats)
    {
      Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
      if (flow.second.rxPackets > 0)
        {
          double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1024;
          std::cout << "  Throughput: " << throughput << " kbps\n";
          std::cout << "  Mean delay: " << (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) << " s\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}