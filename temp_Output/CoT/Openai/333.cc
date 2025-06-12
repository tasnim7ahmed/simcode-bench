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
  Ipv4Address serverAddress = interfaces.GetAddress (1);

  uint16_t port = 8080;

  // Server application: PacketSink on port 8080
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApps = packetSinkHelper.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Client application: BulkSendApplication sending 10 * 1024 bytes in total, at 1s interval for each packet
  // We'll use OnOffApplication for precise packet/interval control.
  OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                            InetSocketAddress (serverAddress, port));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (10240));
  clientHelper.SetAttribute ("DataRate", StringValue ("8192bps")); // 1024 bytes * 8 bits / second

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}