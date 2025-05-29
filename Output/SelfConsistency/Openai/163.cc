#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NonPersistentTcpExample");

int
main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("NonPersistentTcpExample", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point device/link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Install network devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up server: install a PacketSinkApplication
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  // Set up client: OnOffApplication with a single burst (non-persistent)
  OnOffHelper client ("ns3::TcpSocketFactory", sinkAddress);
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  client.SetAttribute ("PacketSize", UintegerValue (1400));

  // Application will send data for a short time and then quit
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (2.0)); // Send for 1 second

  // NetAnim visualization
  AnimationInterface anim ("tcp-nonpersistent.xml");

  // Set positions for nodes (for a better NetAnim visualization)
  anim.SetConstantPosition (nodes.Get (0), 20.0, 30.0);
  anim.SetConstantPosition (nodes.Get (1), 80.0, 30.0);

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}