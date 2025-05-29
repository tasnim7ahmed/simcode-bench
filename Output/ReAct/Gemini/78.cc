#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipSimple");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool printRoutingTables = false;
  bool pingVisibility = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables every 10 seconds", printRoutingTables);
  cmd.AddValue ("pingVisibility", "Make Ping application visible on console", pingVisibility);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RipSimple", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4RoutingTable", LOG_LEVEL_ALL);
    }

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer a, b, c, d;
  a.Create (1);
  b.Create (1);
  c.Create (1);
  d.Create (1);

  InternetStackHelper internet;
  internet.Install (nodes);
  internet.Install (a);
  internet.Install (b);
  internet.Install (c);
  internet.Install (d);

  RipHelper routing;
  routing.SetSplitHorizon (true);
  routing.SetPoisonReverse (true);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NetDeviceContainer devices0, devices1, devices2, devices3, devices4, devices5, devices6;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  devices0 = p2p.Install (nodes.Get (0), a.Get (0));
  devices1 = p2p.Install (nodes.Get (1), d.Get (0));
  devices2 = p2p.Install (a.Get (0), b.Get (0));
  devices3 = p2p.Install (b.Get (0), c.Get (0));
  devices4 = p2p.Install (c.Get (0), d.Get (0));
  p2p.SetChannelAttribute ("Delay", StringValue ("20ms"));
  devices5 = p2p.Install (a.Get (0), c.Get (0));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  devices6 = p2p.Install (b.Get (0), d.Get (0));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign (devices0);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = ipv4.Assign (devices1);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (devices2);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3 = ipv4.Assign (devices3);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  ipv4.Assign (devices4);

  ipv4.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i5 = ipv4.Assign (devices5);

  ipv4.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer i6 = ipv4.Assign (devices6);

  routing.AddInterface (a.Get (0), i0.GetAddress (1));
  routing.AddInterface (a.Get (0), i2.GetAddress (0));
  routing.AddInterface (a.Get (0), i5.GetAddress (0));
  routing.AddInterface (b.Get (0), i2.GetAddress (1));
  routing.AddInterface (b.Get (0), i3.GetAddress (0));
  routing.AddInterface (b.Get (0), i6.GetAddress (0));
  routing.AddInterface (c.Get (0), i3.GetAddress (1));
  routing.AddInterface (c.Get (0), i5.GetAddress (1));
  routing.AddInterface (d.Get (0), i1.GetAddress (1));
  routing.AddInterface (d.Get (0), i6.GetAddress (1));

  routing.SetMetric (c.Get(0), i1.GetAddress(1), 10);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ipv4Address dstAddr = i1.GetAddress (0);
  uint16_t port = 9;

  UdpClientHelper client (dstAddr, port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (100.0));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (100.0));

  if (pingVisibility)
    {
      Packet::EnablePrinting ();
      Packet::EnableIpv4Printing ();
    }

  V4PingHelper ping (i1.GetAddress (0));
  ping.SetAttribute ("Verbose", BooleanValue (pingVisibility));
  ping.SetAttribute ("Interval", TimeValue (Seconds (1)));

  ApplicationContainer pingApps;
  pingApps.Add (ping.Install (nodes.Get (0)));
  pingApps.Start (Seconds (10));
  pingApps.Stop (Seconds (100));

  if (printRoutingTables)
    {
      Simulator::Schedule (Seconds (10.0), &Ipv4RoutingTable::PrintRoutingTableAllAt);
    }

  Simulator::Schedule (Seconds (40.0), [&] () {
      p2p.SetDeviceAttribute ("DataRate", StringValue ("0Mbps"));
      p2p.SetChannelAttribute ("Delay", StringValue ("1000s"));
      devices6 = p2p.Install (b.Get (0), d.Get (0));
    });

  Simulator::Schedule (Seconds (44.0), [&] () {
      p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
      devices6 = p2p.Install (b.Get (0), d.Get (0));
      ipv4.SetBase ("10.1.7.0", "255.255.255.0");
      Ipv4InterfaceContainer i6 = ipv4.Assign (devices6);
      routing.AddInterface (b.Get (0), i6.GetAddress (0));
      routing.AddInterface (d.Get (0), i6.GetAddress (1));
  });

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}