#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopology");

int
main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("Packet::DropReason", StringValue("Drop"));
  LogComponent::Enable ("UdpClient");
  LogComponent::Enable ("UdpServer");

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Creating nodes.");
  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2));

  NS_LOG_INFO ("Creating channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer d0d1 = pointToPoint.Install (n0n1);
  NetDeviceContainer d0d2 = pointToPoint.Install (n0n2);

  NS_LOG_INFO ("Installing internet stack on nodes.");
  InternetStackHelper internet;
  internet.Install (nodes);

  NS_LOG_INFO ("Assigning IP addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign (d0d2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  uint16_t port = 9;  // well-known echo port number
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i0i2.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}