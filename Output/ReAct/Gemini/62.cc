#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPacingExample");

int main (int argc, char *argv[])
{
  bool enablePacing = true;
  double simulationTime = 10.0;
  std::string dataRate = "10Mbps";
  std::string delay = "10ms";
  uint32_t packetSize = 1024;
  uint32_t initialCwnd = 10;

  CommandLine cmd;
  cmd.AddValue ("enablePacing", "Enable TCP pacing", enablePacing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("dataRate", "Bottleneck link data rate", dataRate);
  cmd.AddValue ("delay", "Bottleneck link delay", delay);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("initialCwnd", "Initial congestion window", initialCwnd);
  cmd.Parse (argc, argv);

  LogComponentEnable ("TcpPacingExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("20ms"));
  NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));
  NetDeviceContainer devices34 = pointToPoint.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer devices35 = pointToPoint.Install (nodes.Get (3), nodes.Get (5));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces34 = address.Assign (devices34);
  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces35 = address.Assign (devices35);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkAddress (Ipv4Address::GetAddressFromString ("10.1.4.2"));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (sinkAddress, port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (4));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (simulationTime + 1));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (2), TcpSocketFactory::GetTypeId ());
  if (enablePacing) {
    ns3TcpSocket->SetAttribute ("UsePacing", BooleanValue (true));
  }
  ns3TcpSocket->SetAttribute ("InitialCwnd", UintegerValue (initialCwnd));

  TypeId tid = TypeId::LookupByName ("ns3::TcpNewReno");
  Config::Set ("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue (tid));

  BulkSendApplicationHelper bulkSendHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces34.GetAddress (1), port));
  bulkSendHelper.SetAttribute ("SendSize", UintegerValue (packetSize));
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp = bulkSendHelper.Install (nodes.Get (2));
  sourceApp.Start (Seconds (2.0));
  sourceApp.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:    " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile ("tcp-pacing.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}