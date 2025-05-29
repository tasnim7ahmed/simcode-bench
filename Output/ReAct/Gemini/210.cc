#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridTopology");

int main (int argc, char *argv[])
{
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "UdpClient", LOG_LEVEL_INFO );
  LogComponent::SetFilter ( "UdpServer", LOG_LEVEL_INFO );

  //
  // Create nodes
  //

  // TCP Star Topology
  NodeContainer tcpNodes;
  tcpNodes.Create (4);

  NodeContainer tcpServerNode = NodeContainer (tcpNodes.Get (0));
  NodeContainer tcpClientNodes = NodeContainer (tcpNodes.Get (1), tcpNodes.Get (2), tcpNodes.Get (3));

  // UDP Mesh Topology
  NodeContainer udpNodes;
  udpNodes.Create (4);

  //
  // Create channels
  //

  // TCP Star Topology
  PointToPointHelper pointToPointTcp;
  pointToPointTcp.SetDeviceAttribute  ("DataRate", StringValue ("5Mbps"));
  pointToPointTcp.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer tcpServerDevices;
  NetDeviceContainer tcpClientDevices;

  for (int i = 1; i < 4; ++i)
    {
      NetDeviceContainer link = pointToPointTcp.Install (tcpNodes.Get (0), tcpNodes.Get (i));
      tcpServerDevices.Add (link.Get (0));
      tcpClientDevices.Add (link.Get (1));
    }
    
  // UDP Mesh Topology using WiFi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer udpDevices = wifi.Install (phy, mac, udpNodes);
    
  //
  // Install Internet Stack
  //

  InternetStackHelper internet;
  internet.Install (tcpNodes);
  internet.Install (udpNodes);

  //
  // Assign IP Addresses
  //

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer tcpInterfaces = address.Assign (tcpServerDevices);
  address.Assign (tcpClientDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer udpInterfaces = address.Assign (udpDevices);

  //
  // Install Applications
  //

  // TCP Server
  uint16_t tcpPort = 9;
  Address tcpLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory", tcpLocalAddress);
  ApplicationContainer tcpSinkApp = tcpSinkHelper.Install (tcpServerNode);
  tcpSinkApp.Start (Seconds (1.0));
  tcpSinkApp.Stop (Seconds (10.0));

  // TCP Clients
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (tcpInterfaces.GetAddress (0), tcpPort));
  source.SetAttribute ("MaxBytes", UintegerValue (1000000));
  ApplicationContainer sourceApps;
  for (int i = 1; i < 4; ++i) {
      sourceApps.Add (source.Install (tcpNodes.Get (i)));
  }
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (10.0));

  // UDP Servers (one on each node)
  uint16_t udpPort = 9;
  UdpEchoServerHelper echoServer (udpPort);
  ApplicationContainer udpServerApps = echoServer.Install (udpNodes);
  udpServerApps.Start (Seconds (1.0));
  udpServerApps.Stop (Seconds (10.0));

  // UDP Clients (each node sends to the next)
  UdpEchoClientHelper echoClient (udpInterfaces.GetAddress (1), udpPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer udpClientApps;
  for(uint32_t i = 0; i < udpNodes.GetN(); ++i) {
      uint32_t destNodeIndex = (i + 1) % udpNodes.GetN();
      echoClient.SetRemoteAddress(udpInterfaces.GetAddress(destNodeIndex));
      udpClientApps.Add(echoClient.Install(udpNodes.Get(i)));
  }
  udpClientApps.Start (Seconds (2.0));
  udpClientApps.Stop (Seconds (10.0));
  
  //
  // Routing
  //

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //
  // Tracing
  //

  if (tracing)
    {
      pointToPointTcp.EnablePcapAll ("hybrid-tcp");
      phy.EnablePcapAll ("hybrid-udp");
    }

  //
  // Flow Monitor
  //
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  //
  // NetAnim
  //

  AnimationInterface anim ("hybrid-topology.xml");
  anim.SetConstantPosition (tcpNodes.Get (0), 10, 10);
  anim.SetConstantPosition (tcpNodes.Get (1), 20, 20);
  anim.SetConstantPosition (tcpNodes.Get (2), 30, 20);
  anim.SetConstantPosition (tcpNodes.Get (3), 40, 20);

  anim.SetConstantPosition (udpNodes.Get (0), 10, 30);
  anim.SetConstantPosition (udpNodes.Get (1), 20, 30);
  anim.SetConstantPosition (udpNodes.Get (2), 30, 30);
  anim.SetConstantPosition (udpNodes.Get (3), 40, 30);

  //
  // Run Simulation
  //

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  //
  // Print Flow Statistics
  //
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}