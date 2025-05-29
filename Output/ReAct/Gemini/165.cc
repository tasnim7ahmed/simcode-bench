#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/ipv4-ospf-routing.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfExample");

void PrintRoutingTable (Ptr<Node> node)
{
  Ipv4RoutingTableEntryList routingTable;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol ();
  routing->GetRoutingTable (routingTable);

  std::cout << "Routing table for Node " << node->GetId() << ":" << std::endl;
  for (Ipv4RoutingTableEntryList::const_iterator i = routingTable.begin (); i != routingTable.end (); ++i)
    {
      Ipv4RoutingTableEntry route = *i;
      std::cout << "  Destination: " << route.GetDestNetwork () << " via " << route.GetGateway () << std::endl;
    }
  std::cout << std::endl;
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetLogLevel ("OspfExample", LogLevel::LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer devices30 = p2p.Install (nodes.Get (3), nodes.Get (0));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = ipv4.Assign (devices01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = ipv4.Assign (devices12);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = ipv4.Assign (devices23);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces30 = ipv4.Assign (devices30);

  OspfHelper ospf;
  ospf.SetArea (0);
  ospf.Install (nodes);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  PrintRoutingTable (nodes.Get (0));
  PrintRoutingTable (nodes.Get (1));
  PrintRoutingTable (nodes.Get (2));
  PrintRoutingTable (nodes.Get (3));

  Simulator::Destroy ();
  return 0;
}