#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ripng-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/netanim-module.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngCustomTopology");

int main (int argc, char **argv)
{
  Time::SetResolution (Time::NS);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (6); // SRC:0, A:1, B:2, C:3, D:4, DST:5

  // Links: SRC-A, A-B, A-C, B-C, B-D, C-D (cost 10), D-DST
  // Links:   n0-n1, n1-n2, n1-n3, n2-n3, n2-n4, n3-n4(cost10), n4-n5
  enum {
    SRC = 0,
    A   = 1,
    B   = 2,
    C   = 3,
    D   = 4,
    DST = 5
  };

  std::vector<NodeContainer> linkNodes;
  linkNodes.push_back (NodeContainer (nodes.Get (SRC), nodes.Get (A)));  // 0: SRC-A
  linkNodes.push_back (NodeContainer (nodes.Get (A),   nodes.Get (B)));  // 1: A-B
  linkNodes.push_back (NodeContainer (nodes.Get (A),   nodes.Get (C)));  // 2: A-C
  linkNodes.push_back (NodeContainer (nodes.Get (B),   nodes.Get (C)));  // 3: B-C
  linkNodes.push_back (NodeContainer (nodes.Get (B),   nodes.Get (D)));  // 4: B-D
  linkNodes.push_back (NodeContainer (nodes.Get (C),   nodes.Get (D)));  // 5: C-D (cost 10)
  linkNodes.push_back (NodeContainer (nodes.Get (D),   nodes.Get (DST))); // 6: D-DST

  std::vector<NetDeviceContainer> deviceContainers (7);

  // All links have the same attributes except C-D: set C-D cost to 10 below
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  for (uint32_t i = 0; i < linkNodes.size (); ++i)
    {
      // use default attributes except C-D
      if (i == 5)
        {
          p2p.SetChannelAttribute ("Delay", StringValue ("2ms")); // keep delay, set link cost later
        }
      else
        {
          p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
        }
      deviceContainers[i] = p2p.Install (linkNodes[i]);
    }

  // Install Internet stack
  InternetStackHelper internetv6;
  RipNgHelper ripNgRouting;
  ripNgRouting.Set ("SplitHorizon", EnumValue (RipNgHelper::SPLIT_HORIZON));
  Ipv6StaticRoutingHelper staticRoutingHelper;

  internetv6.SetIpv4StackInstall (false);

  // Routers: A, B, C, D; End hosts: SRC, DST
  // Routers: enable RIPng
  NodeContainer routers;
  routers.Add (nodes.Get (A));
  routers.Add (nodes.Get (B));
  routers.Add (nodes.Get (C));
  routers.Add (nodes.Get (D));
  internetv6.SetRoutingHelper (ripNgRouting);
  internetv6.Install (routers);

  // End hosts: static routing
  NodeContainer hosts;
  hosts.Add (nodes.Get (SRC));
  hosts.Add (nodes.Get (DST));
  internetv6.SetRoutingHelper (staticRoutingHelper);
  internetv6.Install (hosts);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  std::vector<Ipv6InterfaceContainer> ifaces (7);

  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  ifaces[0] = ipv6.Assign (deviceContainers[0]); // SRC-A
  ipv6.NewNetwork ();
  ifaces[1] = ipv6.Assign (deviceContainers[1]); // A-B
  ipv6.NewNetwork ();
  ifaces[2] = ipv6.Assign (deviceContainers[2]); // A-C
  ipv6.NewNetwork ();
  ifaces[3] = ipv6.Assign (deviceContainers[3]); // B-C
  ipv6.NewNetwork ();
  ifaces[4] = ipv6.Assign (deviceContainers[4]); // B-D
  ipv6.NewNetwork ();
  ifaces[5] = ipv6.Assign (deviceContainers[5]); // C-D
  ipv6.NewNetwork ();
  ifaces[6] = ipv6.Assign (deviceContainers[6]); // D-DST

  // Set interface metrics for C-D to 10
  Ptr<Ipv6> ipv6C = nodes.Get (C)->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6D = nodes.Get (D)->GetObject<Ipv6> ();
  uint32_t cIfIdx = ifaces[5].GetInterfaceIndex (0);
  uint32_t dIfIdx = ifaces[5].GetInterfaceIndex (1);
  ipv6C->SetMetric (cIfIdx, 10);
  ipv6D->SetMetric (dIfIdx, 10);

  // Assign static addresses to A and D on all interfaces
  // Since the interfaces already have addresses, nothing more is required, except routing if needed

  // Enable global routing to allow manually added static routes if necessary
  Ipv6GlobalRoutingHelper::PopulateRoutingTables ();

  // Ping6 app on SRC to DST
  uint32_t packetSize = 56;
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds (1.0);
  // Find destination address (D-DST, interface 1 of D-DST)
  Ipv6Address dstAddr = ifaces[6].GetAddress (1, 1); // 2nd device, global address (index=1)
  Ptr<Node> srcNode = nodes.Get (SRC);

  // Wait until after 3s to start, so that routing tables are converged and all interfaces become UP
  Ping6Helper ping6;
  ping6.SetAttribute ("RemoteAddress", Ipv6AddressValue (dstAddr));
  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer papps = ping6.Install (srcNode);
  papps.Start (Seconds (3.0));
  papps.Stop (Seconds (55.0));

  // Tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("ripng-trace.tr"));
  p2p.EnablePcapAll ("ripng");

  // Animation (optional)
  // AnimationInterface anim ("ripng-topo.xml");

  // Simulate link break between B-D at 40s, recover at 44s
  Ptr<Node> bNode = nodes.Get (B);
  Ptr<Node> dNode = nodes.Get (D);
  Ptr<NetDevice> bDev = deviceContainers[4].Get (0);
  Ptr<NetDevice> dDev = deviceContainers[4].Get (1);

  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, bDev);
  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, dDev);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, bDev);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, dDev);

  Simulator::Stop (Seconds (55.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}