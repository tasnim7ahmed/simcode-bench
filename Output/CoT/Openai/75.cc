#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void ReceivePacket (Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream rxTrace;
  static bool initialized = false;
  if (!initialized)
    {
      rxTrace.open ("rx-trace.txt");
      initialized = true;
    }
  rxTrace << Simulator::Now ().GetSeconds () << " Received packet of size " << packet->GetSize () << " bytes" << std::endl;
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer n0n2;
  n0n2.Add (CreateObject<Node> ()); // n0
  n0n2.Add (CreateObject<Node> ()); // n2

  NodeContainer n1n2;
  n1n2.Add (CreateObject<Node> ()); // n1
  n1n2.Add (n0n2.Get (1));          // n2 (same as above)

  NodeContainer csmaNodes;
  csmaNodes.Add (n0n2.Get (1)); // n2
  csmaNodes.Create (4);         // n3, n4, n5, n6->will use n3,n4,n5

  Ptr<Node> n0 = n0n2.Get (0);
  Ptr<Node> n1 = n1n2.Get (0);
  Ptr<Node> n2 = n0n2.Get (1);
  Ptr<Node> n3 = csmaNodes.Get (1);
  Ptr<Node> n4 = csmaNodes.Get (2);
  Ptr<Node> n5 = csmaNodes.Get (3);

  NodeContainer n5n6;
  n5n6.Add (n5); // n5 (already made)
  n5n6.Create (1); // n6
  Ptr<Node> n6 = n5n6.Get (1);

  // Point-to-point from n0<->n2 and n1<->n2
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0n2 = p2p.Install (n0n2);
  NetDeviceContainer d1n2 = p2p.Install (n1n2);

  // CSMA bus for n2, n3, n4, n5
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NodeContainer csmaGroup;
  csmaGroup.Add (n2);
  csmaGroup.Add (n3);
  csmaGroup.Add (n4);
  csmaGroup.Add (n5);

  NetDeviceContainer csmaDevs = csma.Install (csmaGroup);

  // Point-to-point n5<->n6
  NetDeviceContainer d5n6 = p2p.Install (n5n6);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (n0);
  stack.Install (n1);
  stack.Install (n2);
  stack.Install (n3);
  stack.Install (n4);
  stack.Install (n5);
  stack.Install (n6);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0n2 = address.Assign (d0n2);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1n2 = address.Assign (d1n2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer icsma = address.Assign (csmaDevs);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i5n6 = address.Assign (d5n6);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // CBR UDP traffic from n0 to n6
  uint16_t sinkPort = 9000;
  Address sinkAddress (InetSocketAddress (i5n6.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (n6);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (n0, UdpSocketFactory::GetTypeId ());

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", StringValue ("448Kbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (210));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

  ApplicationContainer app = onoff.Install (n0);

  // Enable tracing of queues
  p2p.EnablePcapAll ("trace-p2p");
  csma.EnablePcapAll ("trace-csma");

  // Trace packet receptions
  sinkApp.Get (0)->GetObject<PacketSink> ()->TraceConnectWithoutContext (
    "Rx", MakeCallback (&ReceivePacket)
  );

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}