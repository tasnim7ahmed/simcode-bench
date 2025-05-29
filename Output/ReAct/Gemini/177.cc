#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpCongestion");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("TcpL4Protocol", "SocketType", StringValue ("ns3::TcpSocketFactory"));

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer leftDevices, rightDevices;
  leftDevices = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  rightDevices = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces = address.Assign (leftDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer rightInterfaces = address.Assign (rightDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  BulkSendHelper source1 ("ns3::TcpSocketFactory", InetSocketAddress (rightInterfaces.GetAddress (0), port));
  source1.SetAttribute ("SendSize", UintegerValue (1024));
  source1.SetAttribute ("MaxBytes", UintegerValue (1000000));
  ApplicationContainer sourceApps1 = source1.Install (nodes.Get (0));
  sourceApps1.Start (Seconds (1.0));
  sourceApps1.Stop (Seconds (10.0));

   BulkSendHelper source2 ("ns3::TcpSocketFactory", InetSocketAddress (rightInterfaces.GetAddress (0), port+1));
  source2.SetAttribute ("SendSize", UintegerValue (1024));
  source2.SetAttribute ("MaxBytes", UintegerValue (1000000));
  ApplicationContainer sourceApps2 = source2.Install (nodes.Get (0));
  sourceApps2.Start (Seconds (1.0));
  sourceApps2.Stop (Seconds (10.0));


  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (2));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port+1));
  ApplicationContainer sinkApps2 = sink2.Install (nodes.Get (2));
  sinkApps2.Start (Seconds (1.0));
  sinkApps2.Stop (Seconds (10.0));


  // Set Reno and Cubic TCP variants
  GlobalValue::Bind ("TcpL4Protocol::SocketType", StringValue ("ns3::TcpRenoSocketFactory"));
  //GlobalValue::Bind ("TcpL4Protocol::SocketType", StringValue ("ns3::TcpCubicSocketFactory")); //uncomment to use Cubic


  AnimationInterface anim ("tcp-congestion.xml");
  anim.SetConstantPosition (nodes.Get (0), 0.0, 1.0);
  anim.SetConstantPosition (nodes.Get (1), 2.0, 1.0);
  anim.SetConstantPosition (nodes.Get (2), 4.0, 1.0);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}