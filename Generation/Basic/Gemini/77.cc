#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/rip-ng-helper.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/ping6-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipNgTopology");

int main (int argc, char *argv[])
{
  LogComponentEnable("RipNg", LOG_LEVEL_INFO);
  LogComponentEnable("RipNgRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

  // Set up simulation default attributes for RIPng
  Config::SetDefault ("ns3::RipNg::UpdateInterval", TimeValue (Seconds (5)));

  // Create nodes
  NodeContainer nodes;
  nodes.Create (6); // SRC, A, B, C, D, DST
  Ptr<Node> src = nodes.Get (0);
  Ptr<Node> a = nodes.Get (1);
  Ptr<Node> b = nodes.Get (2);
  Ptr<Node> c = nodes.Get (3);
  Ptr<Node> d = nodes.Get (4);
  Ptr<Node> dst = nodes.Get (5);

  // Install Internet stacks with RIPng for routers (A, B, C, D)
  RipNgHelper ripNgRouter;
  // Explicitly enable Split Horizon (it's true by default, but set for clarity)
  ripNgRouter.SetAttribute ("SplitHorizon", BooleanValue (true));
  
  InternetStackHelper internetRipNgStackHelper;
  internetRipNgStackHelper.SetIpv6RoutingHelper (ripNgRouter);
  internetRipNgStackHelper.Install (NodeContainer (a, b, c, d));

  // Install Internet stacks with static routing for end hosts (SRC, DST)
  InternetStackHelper internetHostStackHelper;
  Ipv6StaticRoutingHelper ipv6StaticRoutingHelper;
  internetHostStackHelper.SetIpv6RoutingHelper (ipv6StaticRoutingHelper);
  internetHostStackHelper.Install (NodeContainer (src, dst));

  // Create PointToPointHelper for links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link containers
  NetDeviceContainer devSrcA, devAB, devAC, devBC, devBD, devCD, devDDst;
  Ipv6InterfaceContainer ifSrcA, ifAB, ifAC, ifBC, ifBD, ifCD, ifDDst;

  // Assign IPv6 addresses for each link
  Ipv6AddressHelper ipv6;

  // SRC <-> A (2001:db8:1::/64)
  devSrcA = p2p.Install (src, a);
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  ifSrcA = ipv6.Assign (devSrcA);

  // A <-> B (2001:db8:2::/64)
  devAB = p2p.Install (a, b);
  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  ifAB = ipv6.Assign (devAB);

  // A <-> C (2001:db8:3::/64)
  devAC = p2p.Install (a, c);
  ipv6.SetBase (Ipv6Address ("2001:db8:3::"), Ipv6Prefix (64));
  ifAC = ipv6.Assign (devAC);

  // B <-> C (2001:db8:4::/64)
  devBC = p2p.Install (b, c);
  ipv6.SetBase (Ipv6Address ("2001:db8:4::"), Ipv6Prefix (64));
  ifBC = ipv6.Assign (devBC);

  // B <-> D (2001:db8:5::/64) - This link will break and recover
  devBD = p2p.Install (b, d);
  ipv6.SetBase (Ipv6Address ("2001:db8:5::"), Ipv6Prefix (64));
  ifBD = ipv6.Assign (devBD);

  // C <-> D (2001:db8:6::/64) - Cost 10
  devCD = p2p.Install (c, d);
  ipv6.SetBase (Ipv6Address ("2001:db8:6::"), Ipv6Prefix (64));
  ifCD = ipv6.Assign (devCD);
  ripNgRouter.SetInterfaceMetric (c, ifCD.GetInterfaceIndex (0), 10); // Interface on C to D
  ripNgRouter.SetInterfaceMetric (d, ifCD.GetInterfaceIndex (1), 10); // Interface on D to C

  // D <-> DST (2001:db8:7::/64)
  devDDst = p2p.Install (d, dst);
  ipv6.SetBase (Ipv6Address ("2001:db8:7::"), Ipv6Prefix (64));
  ifDDst = ipv6.Assign (devDDst);

  // Add static routes for SRC and DST hosts
  Ptr<Ipv6StaticRouting> srcStaticRouting = ipv6StaticRoutingHelper.Get (src->GetObject<Ipv6ListRouting> ());
  Ptr<Ipv6StaticRouting> dstStaticRouting = ipv6StaticRoutingHelper.Get (dst->GetObject<Ipv6ListRouting> ());

  // SRC's default route points to A's address on the SRC-A link
  srcStaticRouting->SetDefaultRoute (ifSrcA.GetAddress (1, 0), ifSrcA.GetInterfaceIndex (0));

  // DST's default route points to D's address on the D-DST link
  dstStaticRouting->SetDefaultRoute (ifDDst.GetAddress (0, 0), ifDDst.GetInterfaceIndex (1));

  // Enable pcap tracing on all point-to-point devices
  p2p.EnablePcapAll ("rip-ng-simulation");

  // Enable ascii tracing on all point-to-point devices
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("rip-ng-simulation.tr"));

  // Configure Ping6 application from SRC to DST
  Ping6Helper ping6;
  ping6.SetTargetAddress (ifDDst.GetAddress (1, 0)); // DST's IPv6 address on the D-DST link
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Count", UintegerValue (100)); // Sufficiently many pings
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer pingApps = ping6.Install (src);
  pingApps.Start (Seconds (3.0)); // Start ping after 3 seconds as stated
  pingApps.Stop (Seconds (49.0)); // Stop before simulation ends

  // Get the NetDevice pointers for the B-D link for future disable/enable
  Ptr<NetDevice> deviceB_D_NodeB = devBD.Get (0);
  Ptr<NetDevice> deviceB_D_NodeD = devBD.Get (1);

  // Schedule link B-D to go down at 40 seconds
  Simulator::Schedule (Seconds (40.0), &NetDevice::SetDown, deviceB_D_NodeB);
  Simulator::Schedule (Seconds (40.0), &NetDevice::SetDown, deviceB_D_NodeD);

  // Schedule link B-D to come back up at 44 seconds
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetUp, deviceB_D_NodeB);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetUp, deviceB_D_NodeD);

  // Set simulation stop time
  Simulator::Stop (Seconds (50.0));

  // Run simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}