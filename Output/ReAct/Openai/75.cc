#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

void
PacketReceiveCallback (Ptr<const Packet> packet, const Address &addr)
{
  static std::ofstream recvStream ("packet-receive-trace.txt", std::ios_base::app);
  recvStream << Simulator::Now ().GetSeconds () << "s Packet received, size: "
             << packet->GetSize () << " bytes" << std::endl;
}

void
QueueTraceCallback (std::string context, uint32_t oldVal, uint32_t newVal)
{
  static std::ofstream queueStream ("queue-trace.txt", std::ios_base::app);
  queueStream << Simulator::Now ().GetSeconds ()
              << "s Queue size changed: old=" << oldVal << " new=" << newVal
              << " (" << context << ")" << std::endl;
}

int
main (int argc, char *argv[])
{
  // Create nodes: n0, n1, n2, n3, n4, n5, n6
  NodeContainer nodes;
  nodes.Create (7);

  // Naming nodes for easier reference
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Node> n2 = nodes.Get (2);
  Ptr<Node> n3 = nodes.Get (3);
  Ptr<Node> n4 = nodes.Get (4);
  Ptr<Node> n5 = nodes.Get (5);
  Ptr<Node> n6 = nodes.Get (6);

  // Point-to-point links: (n0,n2), (n1,n2), (n5,n6)
  NodeContainer p2p0 (n0, n2);
  NodeContainer p2p1 (n1, n2);
  NodeContainer p2p2 (n5, n6);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d_p2p0 = p2p.Install (p2p0);
  NetDeviceContainer d_p2p1 = p2p.Install (p2p1);
  NetDeviceContainer d_p2p2 = p2p.Install (p2p2);

  // CSMA/CD bus: n2, n3, n4, n5
  NodeContainer csmaNodes;
  csmaNodes.Add (n2);
  csmaNodes.Add (n3);
  csmaNodes.Add (n4);
  csmaNodes.Add (n5);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer d_csma = csma.Install (csmaNodes);

  // Internet stack and IP assignment
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;

  // Assign IPs
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_p2p0 = address.Assign (d_p2p0);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_p2p1 = address.Assign (d_p2p1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_csma = address.Assign (d_csma);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i_p2p2 = address.Assign (d_p2p2);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP packet sink on n6
  uint16_t sinkPort = 9000;
  Address sinkAddress (InetSocketAddress (i_p2p2.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (n6);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (12.0));
  
  // Install OnOffApplication (CBR) at n0, sending to n6
  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetConstantRate (DataRate ("448kbps"), 210);
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (11.0)));
  ApplicationContainer clientApp = onoff.Install (n0);

  // Tracing configuration
  for (uint32_t i = 0; i < d_p2p0.GetN (); ++i)
    {
      Ptr<Queue<Packet> > queue = d_p2p0.Get (i)->GetObject<Queue<Packet> > ();
      if (queue)
        {
          queue->TraceConnect ("PacketsInQueue", std::to_string (i), MakeCallback (&QueueTraceCallback));
        }
    }
  for (uint32_t i = 0; i < d_p2p1.GetN (); ++i)
    {
      Ptr<Queue<Packet> > queue = d_p2p1.Get (i)->GetObject<Queue<Packet> > ();
      if (queue)
        {
          queue->TraceConnect ("PacketsInQueue", std::to_string (i + 2), MakeCallback (&QueueTraceCallback));
        }
    }
  for (uint32_t i = 0; i < d_p2p2.GetN (); ++i)
    {
      Ptr<Queue<Packet> > queue = d_p2p2.Get (i)->GetObject<Queue<Packet> > ();
      if (queue)
        {
          queue->TraceConnect ("PacketsInQueue", std::to_string (i + 4), MakeCallback (&QueueTraceCallback));
        }
    }

  // Trace packet reception at n6 (PacketSink)
  sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketReceiveCallback));

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}