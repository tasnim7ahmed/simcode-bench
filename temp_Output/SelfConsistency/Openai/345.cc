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
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // 2. Set up point-to-point channel attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = pointToPoint.Install (nodes);

  // 3. Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // 4. Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 5. Setup TCP Server (PacketSink at node 1, listening on port 9)
  uint16_t port = 9;
  Address bindAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", bindAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // 6. Setup TCP Client (BulkSendApplication at node 0)
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (512));
  clientHelper.SetAttribute ("DataRate", StringValue ("40960bps")); // 512B*8/0.1s = 40960bps
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (512 * 10)); // 10 packets

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0)); // Start after server
  clientApp.Stop (Seconds (10.0));

  // 7. Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}