#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);
  LogComponent::Enable("OspfExample");

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices3 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer devices4 = pointToPoint.Install (nodes.Get (3), nodes.Get (0));
  NetDeviceContainer devices5 = pointToPoint.Install (nodes.Get (1), nodes.Get (3));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.SetRoutingHelper (ospf);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign (devices3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign (devices4);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces5 = address.Assign (devices5);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces2.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4RoutingTable> routingTable0 = nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol()->GetRoutingTable();
  Ptr<Ipv4RoutingTable> routingTable1 = nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol()->GetRoutingTable();
  Ptr<Ipv4RoutingTable> routingTable2 = nodes.Get(2)->GetObject<Ipv4>()->GetRoutingProtocol()->GetRoutingTable();
  Ptr<Ipv4RoutingTable> routingTable3 = nodes.Get(3)->GetObject<Ipv4>()->GetRoutingProtocol()->GetRoutingTable();

  std::cout << "Routing Table for Node 0:" << std::endl;
  routingTable0->PrintRoutingTable(std::cout);

  std::cout << "Routing Table for Node 1:" << std::endl;
  routingTable1->PrintRoutingTable(std::cout);

  std::cout << "Routing Table for Node 2:" << std::endl;
  routingTable2->PrintRoutingTable(std::cout);

  std::cout << "Routing Table for Node 3:" << std::endl;
  routingTable3->PrintRoutingTable(std::cout);

  Simulator::Destroy ();
  return 0;
}