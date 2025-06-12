#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipSimplePoisonReverse");

int main (int argc, char **argv)
{
  bool verbose = false;
  bool printRoutingTables = false;
  bool showPings = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "turn on log components", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables at 10s intervals", printRoutingTables);
  cmd.AddValue ("showPings", "Show Ping6 reception", showPings);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RipSimplePoisonReverse", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ping", LOG_LEVEL_INFO);
    }

  // Create nodes: SRC, A, B, C, D, DST
  NodeContainer nSrc;  nSrc.Create (1);
  NodeContainer nDst;  nDst.Create (1);
  NodeContainer nA;    nA.Create (1);
  NodeContainer nB;    nB.Create (1);
  NodeContainer nC;    nC.Create (1);
  NodeContainer nD;    nD.Create (1);

  //                      0     1     2   3   4   5
  NodeContainer nodes; //SRC,   A,    B,  C,  D,  DST
  nodes.Add (nSrc.Get (0));
  nodes.Add (nA.Get (0));
  nodes.Add (nB.Get (0));
  nodes.Add (nC.Get (0));
  nodes.Add (nD.Get (0));
  nodes.Add (nDst.Get (0));

  // Link containers
  NodeContainer srcA   (nSrc.Get(0), nA.Get(0));
  NodeContainer aB     (nA.Get(0),   nB.Get(0));
  NodeContainer bD     (nB.Get(0),   nD.Get(0));
  NodeContainer aC     (nA.Get(0),   nC.Get(0));
  NodeContainer cD     (nC.Get(0),   nD.Get(0));
  NodeContainer dDst   (nD.Get(0),   nDst.Get(0));

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetChannelAttribute  ("Delay", StringValue ("2ms"));
  p2p.SetDeviceAttribute   ("DataRate", StringValue ("5Mbps"));

  // Assign extra cost to C-D
  PointToPointHelper p2pCost10;
  p2pCost10.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pCost10.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));

  // Install devices
  NetDeviceContainer d_srcA    = p2p.Install (srcA);
  NetDeviceContainer d_aB      = p2p.Install (aB);
  NetDeviceContainer d_bD      = p2p.Install (bD);
  NetDeviceContainer d_aC      = p2p.Install (aC);
  NetDeviceContainer d_cD      = p2pCost10.Install (cD);
  NetDeviceContainer d_dDst    = p2p.Install (dDst);

  // Internet stack (Install RIP on routers, static on SRC/DST)
  InternetStackHelper internet;
  RipHelper rip;
  rip.ExcludeInterface (nA, 0);    // Don't run RIP on SRC interface
  rip.ExcludeInterface (nB, 0);
  rip.ExcludeInterface (nC, 0);
  rip.ExcludeInterface (nD, 0);
  rip.Set ("SplitHorizon", EnumValue (RipHelper::POISON_REVERSE));

  internet.SetRoutingHelper (rip);
  NodeContainer routers;
  routers.Add (nA); routers.Add (nB); routers.Add (nC); routers.Add (nD);
  internet.Install (routers);

  InternetStackHelper staticStack;
  staticStack.Install (nSrc);
  staticStack.Install (nDst);

  // IP address assignment
  Ipv4AddressHelper ipv4;
  // Prefix layout:
  // Net 10.0.0.0/30 -- SRC-A
  // Net 10.0.1.0/30 -- A-B
  // Net 10.0.2.0/30 -- B-D
  // Net 10.0.3.0/30 -- A-C
  // Net 10.0.4.0/30 -- C-D (cost 10)
  // Net 10.0.5.0/30 -- D-DST

  // SRC-A
  ipv4.SetBase ("10.0.0.0", "255.255.255.252");
  Ipv4InterfaceContainer i_srcA = ipv4.Assign (d_srcA);

  // A-B
  ipv4.SetBase ("10.0.1.0", "255.255.255.252");
  Ipv4InterfaceContainer i_aB = ipv4.Assign (d_aB);

  // B-D
  ipv4.SetBase ("10.0.2.0", "255.255.255.252");
  Ipv4InterfaceContainer i_bD = ipv4.Assign (d_bD);

  // A-C
  ipv4.SetBase ("10.0.3.0", "255.255.255.252");
  Ipv4InterfaceContainer i_aC = ipv4.Assign (d_aC);

  // C-D
  ipv4.SetBase ("10.0.4.0", "255.255.255.252");
  Ipv4InterfaceContainer i_cD = ipv4.Assign (d_cD);

  // D-DST
  ipv4.SetBase ("10.0.5.0", "255.255.255.252");
  Ipv4InterfaceContainer i_dDst = ipv4.Assign (d_dDst);

  // Set interface metrics (cost): default=1; set C-D interface to 10
  Ptr<Ipv4> ipv4C = nC.Get(0)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4D = nD.Get(0)->GetObject<Ipv4> ();

  // The interface index for c-d
  // nC: 0: loopback, 1: A-C, 2: C-D
  // nD: 0: loopback, 1: B-D, 2: C-D, 3: D-DST
  // set metric to 10 on both routers on c-d link
  ipv4C->SetMetric (2, 10);
  ipv4D->SetMetric (2, 10);

  // Enable global routing (for static routes)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up static routes on SRC and DST
  Ipv4StaticRoutingHelper staticRoutingHelper;

  Ptr<Ipv4StaticRouting> staticRoutingSrc = staticRoutingHelper.GetStaticRouting (nSrc.Get(0)->GetObject<Ipv4> ());
  staticRoutingSrc->SetDefaultRoute (i_srcA.GetAddress(1), 1);

  Ptr<Ipv4StaticRouting> staticRoutingDst = staticRoutingHelper.GetStaticRouting (nDst.Get(0)->GetObject<Ipv4> ());
  staticRoutingDst->SetDefaultRoute (i_dDst.GetAddress(0), 1);

  // ICMP Echo application from SRC -> DST
  uint32_t packetSize = 56;
  uint32_t maxPackets = 100;
  Time interPacketInterval = Seconds (1.0);

  V4PingHelper ping (i_dDst.GetAddress(1));
  ping.SetAttribute ("Verbose", BooleanValue (showPings));
  ping.SetAttribute ("Size", UintegerValue (packetSize));
  ping.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping.SetAttribute ("Count", UintegerValue (maxPackets));
  ApplicationContainer apps = ping.Install (nSrc.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (60.0));

  // Print routing tables at intervals
  if (printRoutingTables)
    {
      RipHelper::PrintRoutingTableAllAt (Seconds (10.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (20.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (30.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (38.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (40.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (42.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (44.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (46.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (50.0), routers);
      RipHelper::PrintRoutingTableAllAt (Seconds (60.0), routers);
    }

  // Simulate B-D link failure at t=40s, recover at t=44s
  // bD: d_bD - [0] = nB's device, [1] = nD's device
  Ptr<NetDevice> bD_dev_b = d_bD.Get (0);
  Ptr<NetDevice> bD_dev_d = d_bD.Get (1);

  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, bD_dev_b);
  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, bD_dev_d);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, bD_dev_b);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, bD_dev_d);

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}