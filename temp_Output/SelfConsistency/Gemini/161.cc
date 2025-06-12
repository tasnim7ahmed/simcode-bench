#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpUdpComparison");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "TcpUdpComparison", LOG_LEVEL_INFO );

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure point-to-point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Create TCP application (sink)
  uint16_t tcpPort = 5000;
  Address sinkLocalAddressTcp (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  PacketSinkHelper packetSinkHelperTcp ("ns3::TcpSocketFactory", sinkLocalAddressTcp);
  ApplicationContainer sinkAppsTcp = packetSinkHelperTcp.Install (nodes.Get (1));
  sinkAppsTcp.Start (Seconds (1.0));
  sinkAppsTcp.Stop (Seconds (10.0));


  // Create TCP application (source)
  OnOffHelper onOffHelperTcp ("ns3::TcpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), tcpPort)));
  onOffHelperTcp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelperTcp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelperTcp.SetAttribute ("DataRate", StringValue ("1Mbps")); // Reduced data rate
  ApplicationContainer clientAppsTcp = onOffHelperTcp.Install (nodes.Get (0));
  clientAppsTcp.Start (Seconds (2.0));
  clientAppsTcp.Stop (Seconds (10.0));


  // Create UDP application (sink)
  uint16_t udpPort = 5001;
  Address sinkLocalAddressUdp (InetSocketAddress (Ipv4Address::GetAny (), udpPort));
  PacketSinkHelper packetSinkHelperUdp ("ns3::UdpSocketFactory", sinkLocalAddressUdp);
  ApplicationContainer sinkAppsUdp = packetSinkHelperUdp.Install (nodes.Get (1));
  sinkAppsUdp.Start (Seconds (1.0));
  sinkAppsUdp.Stop (Seconds (10.0));


  // Create UDP application (source)
  OnOffHelper onOffHelperUdp ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), udpPort)));
  onOffHelperUdp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelperUdp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelperUdp.SetAttribute ("DataRate", StringValue ("1Mbps")); // Reduced data rate
  ApplicationContainer clientAppsUdp = onOffHelperUdp.Install (nodes.Get (0));
  clientAppsUdp.Start (Seconds (2.0));
  clientAppsUdp.Stop (Seconds (10.0));

  // Create FlowMonitor
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll ("tcp-udp-comparison");

  // Enable NetAnim
  AnimationInterface anim ("tcp-udp-comparison.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 100, 10);

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Analyze results
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000  << " Mbps\n";
      std::cout << "  Packet Loss:" << i->second.lostPackets << "\n";

    }

  flowMonitor->SerializeToXmlFile("tcp-udp-comparison.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}