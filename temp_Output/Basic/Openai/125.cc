#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"

using namespace ns3;

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  std::ostringstream oss;
  ipv4RoutingHelper.PrintRoutingTable (ipv4, oss);
  std::cout << "Routing table of node " << node->GetId ()
            << " at time " << printTime.GetSeconds () << "s:\n"
            << oss.str ();
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer central;
  central.Create (1);
  NodeContainer edges;
  edges.Create (4);

  NodeContainer starNodes;
  starNodes.Add (central.Get (0));
  for (uint32_t i = 0; i < 4; ++i)
    {
      starNodes.Add (edges.Get (i));
    }

  // Create point-to-point links between central and edge nodes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer centralDevices[4];
  NetDeviceContainer edgeDevices[4];

  for (uint32_t i = 0; i < 4; ++i)
    {
      NodeContainer pair (central.Get (0), edges.Get (i));
      NetDeviceContainer link = p2p.Install (pair);
      centralDevices[i].Add (link.Get (0));
      edgeDevices[i].Add (link.Get (1));
    }

  // Install Internet stack
  RipHelper ripRouting;
  ripRouting.ExcludeInterface (central, 1); // for the record, excludes loopback

  InternetStackHelper stack;
  stack.SetRoutingHelper (ripRouting);

  // Install on all nodes
  for (uint32_t i = 0; i < 5; ++i)
    {
      stack.Install (starNodes.Get (i));
    }

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces (4);

  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i+1) << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces[i] = address.Assign (NetDeviceContainer (centralDevices[i].Get (0), edgeDevices[i].Get (0)));
    }

  // Enable routing table logging periodically
  for (uint32_t t = 0; t <= 40; t += 10)
    {
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, central.Get (0), Seconds (t));
      for (uint32_t i = 0; i < 4; ++i)
        {
          Simulator::Schedule (Seconds (t), &PrintRoutingTable, edges.Get (i), Seconds (t));
        }
    }

  Simulator::Stop (Seconds (45.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}