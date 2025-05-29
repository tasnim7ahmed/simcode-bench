/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HubTrunkVlanSimulation");

int
main (int argc, char *argv[])
{
  // Enable logging for debugging, if needed
  // LogComponentEnable("HubTrunkVlanSimulation", LOG_LEVEL_INFO);

  // Set simulation parameters
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // There are 4 nodes:
  // n0, n1, n2 each in separate subnets (as if on VLANs)
  // n3 is the hub/router
  NodeContainer terminals;
  terminals.Create (3); // n0, n1, n2

  Ptr<Node> router = CreateObject<Node> (); // n3

  // Each terminal connects to the router via its own CSMA segment (emulating a trunk port)
  NetDeviceContainer devTerm0, devRouter0;
  NetDeviceContainer devTerm1, devRouter1;
  NetDeviceContainer devTerm2, devRouter2;

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Subnet 1: n0 <-> router
  NodeContainer subnet0;
  subnet0.Add (terminals.Get (0));
  subnet0.Add (router);
  NetDeviceContainer subnet0Devs = csma.Install (subnet0);
  devTerm0.Add (subnet0Devs.Get (0));
  devRouter0.Add (subnet0Devs.Get (1));

  // Subnet 2: n1 <-> router
  NodeContainer subnet1;
  subnet1.Add (terminals.Get (1));
  subnet1.Add (router);
  NetDeviceContainer subnet1Devs = csma.Install (subnet1);
  devTerm1.Add (subnet1Devs.Get (0));
  devRouter1.Add (subnet1Devs.Get (1));

  // Subnet 3: n2 <-> router
  NodeContainer subnet2;
  subnet2.Add (terminals.Get (2));
  subnet2.Add (router);
  NetDeviceContainer subnet2Devs = csma.Install (subnet2);
  devTerm2.Add (subnet2Devs.Get (0));
  devRouter2.Add (subnet2Devs.Get (1));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (terminals);
  internet.Install (router);

  // Assign IP addresses to the interfaces (VLAN-like subnets)
  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer ifaceTerm0, ifaceRouter0;
  Ipv4InterfaceContainer ifaceTerm1, ifaceRouter1;
  Ipv4InterfaceContainer ifaceTerm2, ifaceRouter2;

  // Subnet 10.1.1.0/24 (n0 <-> router)
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer subnet0Ifaces = ipv4.Assign (subnet0Devs);
  ifaceTerm0.Add (subnet0Ifaces.Get (0)); // n0
  ifaceRouter0.Add (subnet0Ifaces.Get (1)); // router

  // Subnet 10.1.2.0/24 (n1 <-> router)
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer subnet1Ifaces = ipv4.Assign (subnet1Devs);
  ifaceTerm1.Add (subnet1Ifaces.Get (0)); // n1
  ifaceRouter1.Add (subnet1Ifaces.Get (1)); // router

  // Subnet 10.1.3.0/24 (n2 <-> router)
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer subnet2Ifaces = ipv4.Assign (subnet2Devs);
  ifaceTerm2.Add (subnet2Ifaces.Get (0)); // n2
  ifaceRouter2.Add (subnet2Ifaces.Get (1)); // router

  // Enable static routing (Router will forward between VLANs/subnets)
  Ipv4StaticRoutingHelper staticRoutingHelper;

  Ptr<Ipv4> routerIpv4 = router->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> routerStaticRouting = staticRoutingHelper.GetStaticRouting (routerIpv4);

  // Each terminal needs a default route pointing to the router in its subnet
  for (uint32_t i = 0; i < 3; ++i)
    {
      Ptr<Node> terminal = terminals.Get (i);
      Ptr<Ipv4> terminalIpv4 = terminal->GetObject<Ipv4> ();
      Ptr<Ipv4StaticRouting> terminalStaticRouting = staticRoutingHelper.GetStaticRouting (terminalIpv4);

      if (i == 0)
        {
          terminalStaticRouting->SetDefaultRoute (ifaceRouter0.GetAddress (0), 1);
        }
      else if (i == 1)
        {
          terminalStaticRouting->SetDefaultRoute (ifaceRouter1.GetAddress (0), 1);
        }
      else if (i == 2)
        {
          terminalStaticRouting->SetDefaultRoute (ifaceRouter2.GetAddress (0), 1);
        }
    }

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable packet capture on all devices
  csma.EnablePcapAll ("hub-trunk-vlan", true);

  // Install applications: n0 (10.1.1.1) sends to n2 (10.1.3.1) (ICMP Echo)
  uint16_t port = 9;

  // Install echo server on n2
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (terminals.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install echo client on n0
  UdpEchoClientHelper echoClient (ifaceTerm2.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));

  ApplicationContainer clientApps = echoClient.Install (terminals.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}