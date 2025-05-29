#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/queue-size.h"
#include "ns3/packet.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6TwoMtu");

static std::ofstream g_trFile;

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  g_trFile << Simulator::Now ().GetSeconds () << " RX_PACKET " << packet->GetSize () << std::endl;
}

void
QueueEnqueueTrace (Ptr<const Packet> packet)
{
  g_trFile << Simulator::Now ().GetSeconds () << " Q_ENQUEUE " << packet->GetSize () << std::endl;
}

void
QueueDequeueTrace (Ptr<const Packet> packet)
{
  g_trFile << Simulator::Now ().GetSeconds () << " Q_DEQUEUE " << packet->GetSize () << std::endl;
}

void
QueueDropTrace (Ptr<const Packet> packet)
{
  g_trFile << Simulator::Now ().GetSeconds () << " Q_DROP " << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  // Open trace file
  g_trFile.open ("fragmentation-ipv6-two-mtu.tr");

  NodeContainer n0, n1, r;
  n0.Create (1);
  n1.Create (1);
  r.Create (1);

  // n0 <-> r (MTU 5000)
  NodeContainer net1 (n0.Get (0), r.Get (0));
  // r <-> n1 (MTU 1500)
  NodeContainer net2 (r.Get (0), n1.Get (0));

  // CSMA Helpers
  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csma1.SetDeviceAttribute ("Mtu", UintegerValue (5000));

  CsmaHelper csma2;
  csma2.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csma2.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  NetDeviceContainer devices1 = csma1.Install (net1);
  NetDeviceContainer devices2 = csma2.Install (net2);

  // Install Internet Stack
  InternetStackHelper internetv6;
  internetv6.Install (n0);
  internetv6.Install (n1);
  internetv6.Install (r);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6Addr1;
  ipv6Addr1.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if1 = ipv6Addr1.Assign (devices1);
  if1.SetForwarding (1, true);
  if1.SetDefaultRouteInAllNodes (1);

  Ipv6AddressHelper ipv6Addr2;
  ipv6Addr2.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if2 = ipv6Addr2.Assign (devices2);
  if2.SetForwarding (0, true); // router interface
  if2.SetDefaultRouteInAllNodes (0);

  // Enable global routing
  Ipv6GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo: n0 sends to n1
  uint16_t echoPort = 9;

  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n1.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (if2.GetAddress (1, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));

  ApplicationContainer clientApps = echoClient.Install (n0.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing: Packet receptions at n1
  devices2.Get (1)->TraceConnectWithoutContext ("MacRx", MakeCallback (&RxTrace));

  // Tracing: Queue events at both CSMA devices on r (the common node)
  Ptr<Queue<Packet>> q1 = DynamicCast<CsmaNetDevice> (devices1.Get (1))->GetQueue ();
  if (q1)
    {
      q1->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      q1->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
      q1->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDropTrace));
    }
  Ptr<Queue<Packet>> q2 = DynamicCast<CsmaNetDevice> (devices2.Get (0))->GetQueue ();
  if (q2)
    {
      q2->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      q2->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
      q2->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDropTrace));
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  g_trFile.close ();

  return 0;
}