#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging for OSPF
  LogComponentEnable ("OspfInterface", LOG_LEVEL_ALL);
  LogComponentEnable ("OspfRouter", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv4OspfRouting", LOG_LEVEL_ALL);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices[1] = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  devices[2] = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  devices[3] = pointToPoint.Install (nodes.Get (3), nodes.Get (0));

  // Install Internet stack
  InternetStackHelper internet;
  OspfHelper ospf;
  internet.SetRoutingHelper (ospf);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = ipv4.Assign (devices[0]);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces[1] = ipv4.Assign (devices[1]);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces[2] = ipv4.Assign (devices[2]);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  interfaces[3] = ipv4.Assign (devices[3]);

  // Create UDP application
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces[2].GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll ("ospf-example");

  // Enable OSPF routing protocol
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Animation Interface
  // NetAnim::AnimationInterface anim("ospf-animation.xml");

  // Run the simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  // Print routing tables
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      if (ipv4 != nullptr)
        {
          std::cout << "Routing table for Node " << i << ":" << std::endl;
          Ipv4RoutingTableEntryList routingTable = ipv4->GetRoutingTable ();
          for (Ipv4RoutingTableEntryList::iterator j = routingTable.begin (); j != routingTable.end (); ++j)
            {
              Ipv4RoutingTableEntry entry = *j;
              std::cout << "  Destination: " << entry.GetDest () << " Next Hop: " << entry.GetGateway () << std::endl;
            }
        }
    }

  Simulator::Destroy ();
  return 0;
}