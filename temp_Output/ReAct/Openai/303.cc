#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 8080;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));

  // Setup TCP server on node 1
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Setup TCP client on node 0
  uint32_t maxBytes = 500000;
  OnOffHelper client ("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute ("DataRate", StringValue ("10Mbps"));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  client.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  client.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  ApplicationContainer clientApp = client.Install (nodes.Get (0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}