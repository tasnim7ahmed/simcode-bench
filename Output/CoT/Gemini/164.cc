#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfNetAnim");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (5);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices03 = pointToPoint.Install (nodes.Get (0), nodes.Get (3));
  NetDeviceContainer devices34 = pointToPoint.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer devices24 = pointToPoint.Install (nodes.Get (2), nodes.Get (4));

  NS_LOG_INFO ("Assign IPv4 Addresses.");
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = ipv4.Assign (devices01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = ipv4.Assign (devices12);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces03 = ipv4.Assign (devices03);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces34 = ipv4.Assign (devices34);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces24 = ipv4.Assign (devices24);

  // Set up OSPF routing
  NS_LOG_INFO ("Set up OSPF routing on the nodes");
  OspfHelper ospfRoutingHelper;
  ospfRoutingHelper.Install (nodes);

  // Create an OnOff application to generate traffic
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;  // well-known echo port number

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces24.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("1Mbps"));
  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Create an UdpEchoServer application to receive traffic
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (11.0));

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("ospf-netanim");
    }

  // Set up NetAnim
  NS_LOG_INFO ("Create animation file.");
  AnimationInterface anim ("ospf-netanim.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 30, 10);
  anim.SetConstantPosition (nodes.Get (2), 50, 10);
  anim.SetConstantPosition (nodes.Get (3), 10, 30);
  anim.SetConstantPosition (nodes.Get (4), 50, 30);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}