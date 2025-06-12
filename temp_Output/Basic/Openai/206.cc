#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/csma-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorStarNetwork");

int main (int argc, char *argv[])
{
  double simTime = 30.0;
  uint32_t numSensors = 5;
  double radius = 25.0;
  Time pktInterval = Seconds (2.0);
  uint32_t pktSize = 40;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.Parse (argc, argv);

  // Create nodes: 5 sensors + 1 sink in center
  NodeContainer nodes;
  nodes.Create (numSensors + 1);

  // Configure mobility: Sink in center, others in star
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // Sink at (0,0)
  for (uint32_t i = 0; i < numSensors; ++i)
    {
      double angle = i * 2 * M_PI / numSensors;
      positionAlloc->Add (Vector (radius * std::cos(angle), radius * std::sin(angle), 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Install IEEE 802.15.4 devices
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devices = lrWpanHelper.Install (nodes);
  lrWpanHelper.AssociateToPan (devices, 0);

  // Install Internet Stack (6LoWPAN disabled for simplicity)
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP server on sink (node 0)
  uint16_t udpPort = 6000;
  UdpServerHelper server (udpPort);
  ApplicationContainer serverApps = server.Install (nodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simTime));

  // UDP clients on sensor nodes
  for (uint32_t i = 1; i <= numSensors; ++i)
    {
      UdpClientHelper client (interfaces.GetAddress (0), udpPort);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      client.SetAttribute ("Interval", TimeValue (pktInterval));
      client.SetAttribute ("PacketSize", UintegerValue (pktSize));
      ApplicationContainer clientApps = client.Install (nodes.Get (i));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simTime));
    }

  // Enable NetAnim
  AnimationInterface anim ("sensor-star.xml");
  anim.SetConstantPosition (nodes.Get (0), 0, 0);
  for (uint32_t i = 1; i <= numSensors; ++i)
    {
      double angle = (i - 1) * 2 * M_PI / numSensors;
      anim.SetConstantPosition (nodes.Get (i), radius * std::cos(angle), radius * std::sin(angle));
      anim.UpdateNodeDescription (nodes.Get(i), "Sensor-" + std::to_string(i));
    }
  anim.UpdateNodeDescription (nodes.Get(0), "Sink");
  anim.EnablePacketMetadata (true);

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> fm = flowmon.InstallAll();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Report metrics
  fm->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = fm->GetFlowStats ();
  uint64_t rxPackets = 0, txPackets = 0;
  double sumDelay = 0.0;
  double throughput = 0.0;
  uint32_t flows = 0;

  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if (t.destinationPort == udpPort && t.destinationAddress == interfaces.GetAddress (0))
        {
          ++flows;
          txPackets += flow.second.txPackets;
          rxPackets += flow.second.rxPackets;
          sumDelay += flow.second.delaySum.GetSeconds ();
          if (flow.second.timeLastRxPacket.GetSeconds () > 0)
              throughput += (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ());
        }
    }

  std::cout << "Total Flows: " << flows << std::endl;
  std::cout << "Tx Packets: " << txPackets << std::endl;
  std::cout << "Rx Packets: " << rxPackets << std::endl;
  std::cout << "Packet Delivery Ratio: " << (txPackets ? (100.0 * rxPackets / txPackets) : 0.0) << "%" << std::endl;
  std::cout << "Average Latency: " << (rxPackets ? (sumDelay / rxPackets) : 0.0) << " s" << std::endl;
  std::cout << "Average Throughput: " << (flows ? (throughput / flows / 1000.0) : 0.0) << " kbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}