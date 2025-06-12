#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes: node 0 is the hub, nodes 1-4 are spokes
  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer spokeNodes;
  spokeNodes.Create (4);

  // Create 4 point-to-point links between hub and each spoke
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices[4];
  NodeContainer pairNodes[4];
  for (uint32_t i = 0; i < 4; ++i)
    {
      pairNodes[i] = NodeContainer (hubNode.Get (0), spokeNodes.Get (i));
      devices[i] = p2p.Install (pairNodes[i]);
      // Enable pcap tracing
      std::ostringstream oss;
      oss << "star-node" << i+1;
      p2p.EnablePcapAll (oss.str());
    }

  // Install the internet stack on all nodes
  InternetStackHelper internet;
  internet.Install (hubNode);
  internet.Install (spokeNodes);

  // Assign IP addresses to each point-to-point link
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[4];
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      interfaces[i] = address.Assign (devices[i]);
    }

  // Install UDP Echo Server on the hub node (node 0)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApp = echoServer.Install (hubNode.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Install UDP Echo Clients on spoke nodes (nodes 1-4)
  for (uint32_t i = 0; i < 4; ++i)
    {
      UdpEchoClientHelper echoClient (interfaces[i].GetAddress (0), echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

      ApplicationContainer clientApp = echoClient.Install (spokeNodes.Get (i));
      clientApp.Start (Seconds (2.0 + i * 0.1)); // stagger start time
      clientApp.Stop (Seconds (10.0));
    }

  // Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Collect and log FlowMonitor statistics
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();

  uint32_t totalTxPackets = 0;
  uint32_t totalRxPackets = 0;
  double totalDelaySum = 0.0;
  uint64_t totalRxBytes = 0;

  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if (t.destinationPort != echoPort)
        continue;

      totalTxPackets += flow.second.txPackets;
      totalRxPackets += flow.second.rxPackets;
      totalDelaySum += flow.second.delaySum.GetSeconds ();
      totalRxBytes += flow.second.rxBytes;

      std::cout << "Flow " << flow.first << ": " << t.sourceAddress << " -> " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
      std::cout << "  Packet Delivery Ratio: "
                << (flow.second.txPackets ? (double)flow.second.rxPackets / flow.second.txPackets : 0) << std::endl;
      std::cout << "  Average Delay: "
                << (flow.second.rxPackets ? flow.second.delaySum.GetSeconds () / flow.second.rxPackets : 0)
                << " seconds" << std::endl;
      std::cout << "  Throughput: "
                << (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds () > 0 ?
                    (double) flow.second.rxBytes * 8.0 /
                    (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1e6
                  : 0)
                << " Mbps" << std::endl;
    }

  if (totalTxPackets > 0)
    {
      std::cout << "\n===== Aggregate Metrics =====" << std::endl;
      std::cout << "Aggregate Packet Delivery Ratio: "
                << (double) totalRxPackets / totalTxPackets << std::endl;
      std::cout << "Aggregate Average Delay: "
                << (totalRxPackets ? totalDelaySum / totalRxPackets : 0) << " seconds" << std::endl;
      std::cout << "Aggregate Throughput: "
                << (Simulator::Now ().GetSeconds () > 0 ?
                    (double) totalRxBytes * 8.0 / Simulator::Now ().GetSeconds () / 1e6
                  : 0)
                << " Mbps" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}