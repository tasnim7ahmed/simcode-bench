#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Set to true to use IPv6", useIpv6);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Configure DropTail queue
  uint32_t queueSize = 20; // packets
  pointToPoint.SetQueue("ns3::DropTailQueue",
                       "MaxPackets", UintegerValue(queueSize));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices3 = pointToPoint.Install (nodes.Get(2), nodes.Get(3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  Ipv6AddressHelper address6;
  if (useIpv6) {
    address6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  } else {
    address.SetBase ("10.1.1.0", "255.255.255.0");
  }

  Ipv4InterfaceContainer interfaces1;
  Ipv4InterfaceContainer interfaces2;
  Ipv4InterfaceContainer interfaces3;
  Ipv6InterfaceContainer interfaces6_1;
  Ipv6InterfaceContainer interfaces6_2;
  Ipv6InterfaceContainer interfaces6_3;

  if (useIpv6) {
      interfaces6_1 = address6.Assign(devices1);
      interfaces6_2 = address6.Assign(devices2);
      interfaces6_3 = address6.Assign(devices3);
  } else {
    interfaces1 = address.Assign (devices1);
    interfaces2 = address.Assign (devices2);
    interfaces3 = address.Assign (devices3);
  }

  if (!useIpv6)
  {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  } else {
      Ipv6GlobalRoutingHelper::PopulateRoutingTables ();
  }

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (useIpv6 ? interfaces6_1.GetAddress (1, 0) : interfaces1.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Trace queue occupancy
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll ("udp-echo.tr");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}