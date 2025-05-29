#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpPointToPointExample");

void
RxPacketCallback (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND ("Server received packet of size " << packet->GetSize () << " bytes at " << Simulator::Now ().GetSeconds () << "s");
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Set up UDP server on node 1
  uint16_t port = 9;
  UdpServerHelper serverHelper (port);
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (2.0));

  // Trace packet reception
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps.Get (0));
  server->TraceConnectWithoutContext ("Rx", MakeCallback (&RxPacketCallback));

  // Set up UDP client on node 0
  uint32_t maxPackets = 1;
  Time interPacketInterval = Seconds (0.1);
  uint32_t packetSize = 16;
  UdpClientHelper clientHelper (interfaces.GetAddress (1), port);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  clientHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (2.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}