#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeP2PTcp");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("ThreeNodeP2PTcp", LOG_LEVEL_INFO);

  // Create three nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Create two point-to-point links: 0<->1 and 1<->2
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer d0d1, d1d2;
  d0d1 = p2p.Install (nodes.Get(0), nodes.Get(1));
  d1d2 = p2p.Install (nodes.Get(1), nodes.Get(2));

  // Enable PCAP tracing
  p2p.EnablePcapAll("three-node-p2p-tcp");

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  // Set up routing - Node 1 is a router
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install TCP server (PacketSink) on Node 2
  uint16_t port = 12345;
  Address sinkAddress(InetSocketAddress(i1i2.GetAddress(1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get(2));
  sinkApp.Start (Seconds(0.5));
  sinkApp.Stop (Seconds(10.0));

  // Install TCP client (BulkSendApplication) on Node 0
  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited data
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get(0));
  clientApp.Start (Seconds(1.0));
  clientApp.Stop (Seconds(10.0));

  // Install FlowMonitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop (Seconds(10.0));
  Simulator::Run ();

  // Output throughput and packet loss statistics
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      if (t.destinationPort == port)
        {
          std::cout << "Flow ID: " << it->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
          std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
          std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
          std::cout << "  Lost Packets: " << (it->second.txPackets - it->second.rxPackets) << std::endl;
          double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ()) / 1e6;
          std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}