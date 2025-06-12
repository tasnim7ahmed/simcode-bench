#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  std::cout << "Node " << node->GetId() << " IPv4 Routing Table at " << printTime.GetSeconds() << "s\n";
  Ipv4RoutingTableEntry entry;
  uint32_t nRoutes = ipv4->GetRoutingProtocol()->GetNRoutes();
  for (uint32_t i=0; i < nRoutes; ++i)
    {
      entry = ipv4->GetRoutingProtocol()->GetRoute(i);
      std::cout << entry << std::endl;
    }
  std::cout << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  // Square topology: 0-1
  //                 | |
  //                 3-2

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  OspfRoutingHelper ospfRoutingHelper;
  stack.SetRoutingHelper(ospfRoutingHelper);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer ifs01, ifs12, ifs23, ifs30;
  address.SetBase("10.1.1.0", "255.255.255.0");
  ifs01 = address.Assign(d01);
  address.SetBase("10.1.2.0", "255.255.255.0");
  ifs12 = address.Assign(d12);
  address.SetBase("10.1.3.0", "255.255.255.0");
  ifs23 = address.Assign(d23);
  address.SetBase("10.1.4.0", "255.255.255.0");
  ifs30 = address.Assign(d30);

  // Monitor routing tables after convergence (e.g., at 5s)
  for(uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get(i), Seconds(5.0));
    }

  Simulator::Stop(Seconds(6.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}