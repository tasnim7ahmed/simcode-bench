#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourNodeNetwork");

int main (int argc, char *argv[])
{
  bool tracing = true;
  std::string queueDisc = "ns3::FifoQueueDisc";
  uint32_t queueSize = 1000;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("queueDisc", "QueueDisc Type", queueDisc);
  cmd.AddValue ("queueSize", "Max Packets allowed on Queue", queueSize);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer d12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("100ms"));

  NetDeviceContainer d13 = pointToPoint.Install (nodes.Get (1), nodes.Get (3));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer d23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = address.Assign (d02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = address.Assign (d12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = address.Assign (d13);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = address.Assign (d23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (i12.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (3));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Simulator::Schedule (Seconds(5.0), [](){
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
  });

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("four-node");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4RoutingProtocol> ipv4Routing = nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol();
  if (ipv4Routing)
  {
    DynamicCast<Ipv4GlobalRouting>(ipv4Routing)->PrintRoutingTable(Create<OutputStreamWrapper>("routing_table.txt", std::ios::out));
  }

  Ptr<Ipv4StaticRouting> staticRouting = Ipv4RoutingHelper::GetStaticRouting (nodes.Get(1)->GetObject<Ipv4> ());
  staticRouting->SetDefaultRoute (i23.GetAddress (0), 1);

  int j = 0;
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      std::cout << "  Flow " << i->first << " (" << i->second.srcAddress << " -> " << i->second.destAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Bytes: " << i->second.lostBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("four-node.flowmon", true, true);
  Simulator::Destroy ();
  return 0;
}