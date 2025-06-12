#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-interface.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/queue.h"
#include "ns3/trace-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationExample");

static void
QueueEnqueueTrace(std::ofstream* ofs, Ptr<const Packet> packet)
{
  *ofs << Simulator::Now ().GetSeconds () << " Queue: Enqueue " << packet->GetSize () << std::endl;
}

static void
QueueDequeueTrace(std::ofstream* ofs, Ptr<const Packet> packet)
{
  *ofs << Simulator::Now ().GetSeconds () << " Queue: Dequeue " << packet->GetSize () << std::endl;
}

static void
RxTrace(std::ofstream* ofs, Ptr<const Packet> packet, const Address &address)
{
  *ofs << Simulator::Now ().GetSeconds () << " Rx: Received packet of " << packet->GetSize () << " bytes" << std::endl;
}

int 
main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv6FragmentationExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Output trace file
  std::ofstream traceFile;
  traceFile.open ("fragmentation-ipv6.tr");

  NodeContainer nodes;
  nodes.Create (2); // n0, n1
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Node> r = CreateObject<Node> (); // router

  NodeContainer n0r (n0, r);
  NodeContainer r1 (r, n1);

  // CSMA helpers and settings
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer d0r = csma.Install (n0r);
  NetDeviceContainer dr1 = csma.Install (r1);

  // Internet stack + IPv6
  InternetStackHelper stack;
  stack.Install (n0);
  stack.Install (n1);
  stack.Install (r);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0r = ipv6.Assign (d0r);
  i0r.SetRouter (1, true);

  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ir1 = ipv6.Assign (dr1);
  ir1.SetRouter (0, true);

  // Bring interfaces up (as link-local auto, global needs to be brought up manually)
  i0r.SetForwarding (0, true);
  i0r.SetForwarding (1, true);

  ir1.SetForwarding (0, true);
  ir1.SetForwarding (1, true);

  i0r.SetDefaultRouteInAllNodes (1);
  ir1.SetDefaultRouteInAllNodes (0);

  // Set up static routing for n1 (in addition to above default routes)
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  staticRoutingN1->SetDefaultRoute (ir1.GetAddress (1, 1), 1);

  // Set up static routing for n0
  Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  staticRoutingN0->SetDefaultRoute (i0r.GetAddress (0, 1), 1);

  // Set up routing in router node
  Ptr<Ipv6StaticRouting> staticRoutingR = ipv6RoutingHelper.GetStaticRouting (r->GetObject<Ipv6> ());
  staticRoutingR->AddNetworkRouteTo (Ipv6Address ("2001:db8:1::"), 64, i0r.GetAddress (1, 1), 1);
  staticRoutingR->AddNetworkRouteTo (Ipv6Address ("2001:db8:2::"), 64, ir1.GetAddress (0, 1), 2);

  // UDP echo server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP echo client on n0, send a large (fragmented) packet
  uint32_t packetSize = 2000; // Make sure this will be fragmented with typical MTU
  UdpEchoClientHelper echoClient (ir1.GetAddress (1,1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = echoClient.Install (n0);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing
  Ptr<Queue<Packet>> queue0 = d0r.Get (0)->GetObject<CsmaNetDevice> ()->GetQueue ();
  Ptr<Queue<Packet>> queue1 = dr1.Get (0)->GetObject<CsmaNetDevice> ()->GetQueue ();
  if (queue0)
    {
      queue0->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&QueueEnqueueTrace, &traceFile));
      queue0->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&QueueDequeueTrace, &traceFile));
    }
  if (queue1)
    {
      queue1->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&QueueEnqueueTrace, &traceFile));
      queue1->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&QueueDequeueTrace, &traceFile));
    }

  // Rx trace on n1
  dr1.Get (1)->TraceConnectWithoutContext (
          "MacRx", MakeBoundCallback (&RxTrace, &traceFile));

  // Enable pcap tracing
  csma.EnablePcapAll ("fragmentation-ipv6", true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  traceFile.close();

  return 0;
}