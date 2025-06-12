#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/netanim-module.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipSimple");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool printRoutingTables = false;
  bool enablePcap = false;
  bool visible = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables every 10 seconds", printRoutingTables);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("visible", "Enable NetAnim visualization", visible);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RipSimple", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ALL);
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

  RipHelper ripRouting;

  ripRouting.ExcludeInterface (a.Get (0), 1);
  ripRouting.ExcludeInterface (b.Get (0), 1);
  ripRouting.ExcludeInterface (c.Get (0), 1);
  ripRouting.ExcludeInterface (d.Get (0), 1);

  Ipv4GlobalRoutingHelper g;

  Ipv4InterfaceContainer interfaces;

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), a.Get (0));
  NetDeviceContainer devices02 = pointToPoint.Install (a.Get (0), b.Get (0));
  NetDeviceContainer devices03 = pointToPoint.Install (b.Get (0), c.Get (0));
  NetDeviceContainer devices04 = pointToPoint.Install (c.Get (0), d.Get (0));
  NetDeviceContainer devices05 = pointToPoint.Install (d.Get (0), nodes.Get (1));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces.Add (address.Assign (devices01));

  address.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces.Add (address.Assign (devices02));

  address.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces.Add (address.Assign (devices03));

  address.SetBase ("10.1.4.0", "255.255.255.0");
  interfaces.Add (address.Assign (devices04));

  address.SetBase ("10.1.5.0", "255.255.255.0");
  interfaces.Add (address.Assign (devices05));

  ripRouting.SetSplitHorizon (true);
  ripRouting.SetPoisonReverse (true);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  TypeId tid = TypeId::LookupByName ("ns3::UdpEchoClient");
  Ptr<UdpEchoClient> echoClientHelper = CreateObject<UdpEchoClient> ();

  UdpEchoClientHelper echoClient (interfaces.GetAddress (9), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (100.0));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (100.0));

  if (printRoutingTables)
    {
      Simulator::Schedule (Seconds (10.0), &Ipv4::PrintRoutingTableAllAt, Seconds (10));
    }

  Simulator::Schedule (Seconds (40.0), &PointToPointHelper::Disable, pointToPoint, devices03.Get (0), devices03.Get (1));
  Simulator::Schedule (Seconds (44.0), &PointToPointHelper::Enable, pointToPoint, devices03.Get (0), devices03.Get (1));

  if(visible)
  {
      AnimationInterface anim ("rip-simple.xml");
      anim.SetConstantPosition (nodes.Get(0), 0.0, 1.0);
      anim.SetConstantPosition (nodes.Get(1), 5.0, 1.0);
      anim.SetConstantPosition (a.Get(0), 1.0, 2.0);
      anim.SetConstantPosition (b.Get(0), 2.0, 2.0);
      anim.SetConstantPosition (c.Get(0), 3.0, 2.0);
      anim.SetConstantPosition (d.Get(0), 4.0, 2.0);
  }

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}