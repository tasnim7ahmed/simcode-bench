#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("0.01"));

  int k = 4;
  int num_pods = k;
  int num_core_switches = (k / 2) * (k / 2);
  int num_aggregation_switches = (k / 2) * k;
  int num_edge_switches = (k / 2) * k;
  int num_hosts_per_edge = k / 2;
  int total_hosts = num_edge_switches * num_hosts_per_edge;

  NodeContainer coreSwitches;
  coreSwitches.Create (num_core_switches);

  NodeContainer aggregationSwitches;
  aggregationSwitches.Create (num_aggregation_switches);

  NodeContainer edgeSwitches;
  edgeSwitches.Create (num_edge_switches);

  NodeContainer hosts;
  hosts.Create (total_hosts);

  InternetStackHelper stack;
  stack.Install (coreSwitches);
  stack.Install (aggregationSwitches);
  stack.Install (edgeSwitches);
  stack.Install (hosts);

  PointToPointHelper p2p;
  p2p.DataRate = DataRate ("10Gbps");
  p2p.Delay = TimeValue (MilliSeconds (1));
  p2p.ChannelDatarate = DataRate ("10Gbps");
  p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

  // Core to Aggregation
  for (int i = 0; i < k / 2; ++i) {
    for (int j = 0; j < k / 2; ++j) {
      for (int p = 0; p < k; ++p) {
        NetDeviceContainer link = p2p.Install (coreSwitches.Get (i * (k / 2) + j), aggregationSwitches.Get (i * k + p));
      }
    }
  }

  // Aggregation to Edge
  for (int p = 0; p < k; ++p) {
    for (int j = 0; j < k / 2; ++j) {
      for (int h = 0; h < k / 2; ++h) {
        NetDeviceContainer link = p2p.Install (aggregationSwitches.Get (p * k + j), edgeSwitches.Get (p * k + h));
      }
    }
  }

  // Edge to Hosts
  for (int e = 0; e < num_edge_switches; ++e) {
    for (int h = 0; h < num_hosts_per_edge; ++h) {
      NetDeviceContainer link = p2p.Install (edgeSwitches.Get (e), hosts.Get (e * num_hosts_per_edge + h));
    }
  }


  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces;

  interfaces = address.Assign (NetDeviceContainer (coreSwitches.GetDevices ()));
  address.NewNetwork ();

  interfaces = address.Assign (NetDeviceContainer (aggregationSwitches.GetDevices ()));
  address.NewNetwork ();

  interfaces = address.Assign (NetDeviceContainer (edgeSwitches.GetDevices ()));
  address.NewNetwork ();

  interfaces = address.Assign (NetDeviceContainer (hosts.GetDevices ()));
  address.NewNetwork ();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create Applications
  uint16_t port = 9;  // well-known echo port number

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (hosts.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (total_hosts + 1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (hosts.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Configure TCP streams between random hosts
  uint16_t sinkPort = 8080;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (hosts.Get (10));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (hosts.Get (0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->Bind ();
  ns3TcpSocket->Connect (InetSocketAddress (interfaces.GetAddress (total_hosts + 10), sinkPort));

  // Create a bulk send application to send data over the TCP socket
  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (total_hosts + 10), sinkPort)));
  bulkSendHelper.SetAttribute ("SendSize", UintegerValue (1024));
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (10000000)); // Send 10 MB
  ApplicationContainer sourceApps = bulkSendHelper.Install (hosts.Get (0));
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (10.0));


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable Tracing for NetAnim
  AnimationInterface anim ("fat-tree.xml");

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("fat-tree.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}