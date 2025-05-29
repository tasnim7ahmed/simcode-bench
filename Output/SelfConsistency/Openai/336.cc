#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging for troubleshooting (optional)
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  // LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

  // Step 1: Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Step 2: Set up Point-to-Point (P2P) link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Step 3: Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Step 4: Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Step 5: Set up TCP server (PacketSink) on node 1 (Computer 2)
  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (20.0));

  // Step 6: Set up TCP client (OnOffApplication) on node 0 (Computer 1)
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("8192bps"))); // 1024 bytes in 1 sec = 8192 bps

  // Limit to send exactly 10 packets of 1024 bytes at 1s interval (total 10 sec traffic)
  uint32_t numPackets = 10;
  // Stop after sending 10th packet: (first at t=1.0s, last at t=10.0s)
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (11.0));

  // Optional: assign a maximum number of bytes to send (10*1024)
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (10240)); // Not effective unless set before Install

  // Step 7: Run simulation
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}