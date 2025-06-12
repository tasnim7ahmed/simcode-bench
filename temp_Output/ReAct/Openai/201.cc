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

  // Create nodes: node 0 is the central hub, nodes 1-4 are peripheral
  NodeContainer starNodes;
  starNodes.Create (5);

  // Creating point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices[4];
  NodeContainer pairs[4];

  for (uint32_t i = 0; i < 4; ++i)
    {
      pairs[i] = NodeContainer (starNodes.Get (0), starNodes.Get (i + 1));
      devices[i] = p2p.Install (pairs[i]);
    }

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (starNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces(4);
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces[i] = address.Assign (devices[i]);
    }

  // Install UDP Echo Server on central node (node 0)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);

  ApplicationContainer serverApps = echoServer.Install (starNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Echo Clients on peripheral nodes (nodes 1-4)
  for (uint32_t i = 0; i < 4; ++i)
    {
      UdpEchoClientHelper echoClient (interfaces[i].GetAddress (0), echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

      ApplicationContainer clientApps = echoClient.Install (starNodes.Get (i + 1));
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (10.0));
    }

  // Enable pcap tracing
  for (uint32_t i = 0; i < 4; ++i)
    {
      p2p.EnablePcapAll ("star-topology", false);
    }

  // Set up FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Retrieve and print flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  uint32_t totalTxPackets = 0;
  uint32_t totalRxPackets = 0;
  double totalDelay = 0;
  double totalThroughput = 0;

  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      if (t.destinationPort != echoPort) continue;
      totalTxPackets += it->second.txPackets;
      totalRxPackets += it->second.rxPackets;
      if (it->second.rxPackets > 0)
        {
          totalDelay += it->second.delaySum.GetSeconds ();
        }
      double throughput =
        it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;
      totalThroughput += throughput;
      std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
      std::cout << "  Throughput: " << throughput << " Mbps\n";
      std::cout << "  Delay sum: " << it->second.delaySum.GetSeconds () << " s\n";
    }

  double pdr = totalTxPackets > 0 ? (static_cast<double> (totalRxPackets) / totalTxPackets) * 100.0 : 0.0;
  double avgDelay = totalRxPackets > 0 ? totalDelay / totalRxPackets : 0.0;
  double avgThroughput = totalThroughput / 4.0;

  std::cout << "\n==== Simulation Summary ====\n";
  std::cout << "Packet Delivery Ratio: " << pdr << " %\n";
  std::cout << "Average Delay: " << avgDelay << " s\n";
  std::cout << "Average Throughput: " << avgThroughput << " Mbps\n";

  Simulator::Destroy ();
  return 0;
}