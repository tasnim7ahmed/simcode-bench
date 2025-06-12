#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorLoop");

int main (int argc, char *argv[])
{
  LogComponent::SetFilter ("DistanceVectorLoop", Level::LOG_PREFIX);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Schedule (Seconds (5.0), [&]() {
      NS_LOG_INFO ("Simulating link failure...");
      pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("0Mbps"));
      pointToPoint.SetChannelAttribute ("Delay", StringValue ("1000s"));
      pointToPoint.Install (nodes);
      NS_LOG_INFO ("Link down.");

  });

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("distance-vector-loop.xml");
  anim.SetConstantPosition (nodes.Get (0), 1, 1);
  anim.SetConstantPosition (nodes.Get (1), 2, 2);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}