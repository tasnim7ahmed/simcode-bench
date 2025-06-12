#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipExample");

int
main (int argc, char *argv[])
{
  bool verbose = false;
  bool printRoutingTables = false;
  bool enablePings = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("printRoutingTables", "Enable routing table printing.", printRoutingTables);
  cmd.AddValue ("enablePings", "Enable ICMP pings.", enablePings);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv4L4Protocol", LOG_LEVEL_ALL);
    }

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);
  NodeContainer src = NodeContainer (nodes.Get (0));
  NodeContainer dst = NodeContainer (nodes.Get (1));

  NodeContainer routerA, routerB, routerC, routerD;
  routerA.Create (1);
  routerB.Create (1);
  routerC.Create (1);
  routerD.Create (1);

  // Create channels
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer srcA = p2p.Install (src, routerA);
  NetDeviceContainer aB = p2p.Install (routerA, routerB);
  NetDeviceContainer aC = p2p.Install (routerA, routerC);
  NetDeviceContainer bD = p2p.Install (routerB, routerD);
  p2p.SetChannelAttribute ("Delay", StringValue ("20ms")); // Higher delay for C-D link
  NetDeviceContainer cD = p2p.Install (routerC, routerD);
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer dDst = p2p.Install (routerD, dst);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);
  internet.Install (routerA);
  internet.Install (routerB);
  internet.Install (routerC);
  internet.Install (routerD);

  // Assign addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iSrcA = ipv4.Assign (srcA);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iAB = ipv4.Assign (aB);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iAC = ipv4.Assign (aC);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iBD = ipv4.Assign (bD);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iCD = ipv4.Assign (cD);

  ipv4.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer iDDst = ipv4.Assign (dDst);

  // Create RIP routing
  Rip ripRouting;
  ripRouting.SetSplitHorizon (true);
  ripRouting.SetSplitHorizonPoisonReverse (true);

  // Install RIP on routers
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  ripRouting.Install (routerA);
  ripRouting.Install (routerB);
  ripRouting.Install (routerC);
  ripRouting.Install (routerD);

  // Set link costs
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Node> nodeC = routerC.Get (0);
  Ptr<Ipv4StaticRouting> routingC = staticRouting.GetStaticRouting (nodeC->GetObject<Ipv4> ());
  routingC->SetInterfaceCost (iCD.GetInterface (0), 10);

  Ptr<Node> nodeD = routerD.Get (0);
  Ptr<Ipv4StaticRouting> routingD = staticRouting.GetStaticRouting (nodeD->GetObject<Ipv4> ());
  routingD->SetInterfaceCost (iCD.GetInterface (1), 10);

  // Create sink application on DST
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (dst);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (100.0));

  // Create echo client application on SRC
  UdpEchoClientHelper echoClient (iDDst.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (src);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (100.0));

  // Add a ping application
  if (enablePings)
    {
      V4PingHelper ping (iDDst.GetAddress (1));
      ping.SetAttribute ("Verbose", BooleanValue (true));
      ApplicationContainer pingApps = ping.Install (src.Get (0));
      pingApps.Start (Seconds (5));
      pingApps.Stop (Seconds (95));
    }

  // Schedule link failure and recovery
  Simulator::Schedule (Seconds (40), &PointToPointHelper::EnableAsciiAll, &p2p, bD.Get (0), bD.Get (1), "rip-example-bd-drop.tr");
  Simulator::Schedule (Seconds (40), &NetDevice::SetDataRate, bD.Get (0), DataRate ("0kbps"));
  Simulator::Schedule (Seconds (44), &NetDevice::SetDataRate, bD.Get (0), DataRate ("5Mbps"));
  Simulator::Schedule (Seconds (44), &PointToPointHelper::EnableAsciiAll, &p2p, bD.Get (0), bD.Get (1), "rip-example-bd-recover.tr");

  // Print routing tables
  if (printRoutingTables)
    {
      Simulator::Schedule (Seconds (30), &Ipv4::PrintRoutingTableAllAt, Seconds (30));
      Simulator::Schedule (Seconds (60), &Ipv4::PrintRoutingTableAllAt, Seconds (60));
      Simulator::Schedule (Seconds (90), &Ipv4::PrintRoutingTableAllAt, Seconds (90));
    }

  Simulator::Stop (Seconds (100));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}