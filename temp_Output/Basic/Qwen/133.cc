#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SplitHorizonDVExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Setup point-to-point links between A <-> B and B <-> C
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer linkAB = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer linkBC = p2p.Install (nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  DvRoutingHelper dv;
  dv.Set ("PeriodicUpdateInterval", TimeValue (Seconds (10)));
  dv.Set ("SettlingTime", TimeValue (Seconds (8)));
  dv.Set ("SplitHorizon", BooleanValue (true));  // Enable Split Horizon

  Ipv4ListRoutingHelper list;
  list.Add (dv, 10);  // priority 10 for DV routing

  stack.SetRoutingHelper (list);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign (linkAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign (linkBC);

  // Set up echo server on Node C
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get(2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  // Set up echo client on Node A
  UdpEchoClientHelper echoClient (interfacesBC.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (20.0));

  // Enable tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("split-horizon-dv.tr"));
  p2p.EnablePcapAll ("split-horizon-dv");

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}