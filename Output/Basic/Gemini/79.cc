#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourNodesSimulation");

int main (int argc, char *argv[])
{
  bool tracing = true;
  uint32_t metric = 1;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("metric", "Routing Metric for n1-n3 link", metric);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer d12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("100ms"));

  NetDeviceContainer d13 = pointToPoint.Install (nodes.Get (1), nodes.Get (3));

  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer d23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = address.Assign (d02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = address.Assign (d12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = address.Assign (d13);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = address.Assign (d23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ipv4 addressn13 = i13.GetAddress (0);
  Ipv4GlobalRoutingHelper::SetStaticCost (nodes.Get (1), nodes.Get (3), metric);

  uint16_t port = 9;

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (i12.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (3));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing == true)
    {
      pointToPoint.EnablePcapAll ("four-nodes");
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}