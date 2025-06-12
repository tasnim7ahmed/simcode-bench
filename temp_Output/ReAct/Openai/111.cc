#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-ping-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include "ns3/radvd-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdSimulation");

void
ReceivePacket (Ptr<const Packet> packet, const Address &addr)
{
  static std::ofstream out ("radvd.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << "s: Packet received, size = "
      << packet->GetSize () << " bytes, from " << Inet6SocketAddress::ConvertFrom (addr).GetIpv6 () << std::endl;
}

void
QueueTrace (Ptr<Queue<Packet>> queue)
{
  static std::ofstream out ("radvd.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << "s: Queue size = "
      << queue->GetNPackets () << " packets" << std::endl;
  Simulator::Schedule (Seconds (0.01), &QueueTrace, queue);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RadvdSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2); // n0, n1
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);

  NodeContainer router;
  router.Create (1);
  Ptr<Node> r = router.Get (0);

  // Left subnet: n0 <-> R
  NodeContainer leftSubnet;
  leftSubnet.Add (n0);
  leftSubnet.Add (r);

  // Right subnet: R <-> n1
  NodeContainer rightSubnet;
  rightSubnet.Add (r);
  rightSubnet.Add (n1);

  // Install CSMA on each subnet
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer leftDevices = csma.Install (leftSubnet);
  NetDeviceContainer rightDevices = csma.Install (rightSubnet);

  // Install Internet stack with IPv6 support
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (nodes);
  internetv6.Install (router);

  // Assign IPv6 addresses (enable auto-configured global addresses)
  Ipv6AddressHelper ipv6Left;
  ipv6Left.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iLeft = ipv6Left.Assign (leftDevices);
  iLeft.SetForwarding (1, true); // R's left interface

  Ipv6AddressHelper ipv6Right;
  ipv6Right.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iRight = ipv6Right.Assign (rightDevices);
  iRight.SetForwarding (0, true); // R's right interface

  // Install Radvd (Router Advertisement Daemon) on R for both subnets
  RadvdHelper radvdHelper;
  // Left interface
  RadvdInterface radvdIfLeft;
  radvdIfLeft.SetInterface(1); // R's left NetDevice index in R (first interface is 0, second is 1)
  radvdIfLeft.AddPrefix ("2001:1::", 64);
  radvdHelper.AddInterface (r, radvdIfLeft);

  // Right interface
  RadvdInterface radvdIfRight;
  radvdIfRight.SetInterface(2); // R's right NetDevice index in R
  radvdIfRight.AddPrefix ("2001:2::", 64);
  radvdHelper.AddInterface (r, radvdIfRight);

  ApplicationContainer radvdApps = radvdHelper.Install (router);
  radvdApps.Start (Seconds (1.0));
  radvdApps.Stop (Seconds (20.0));

  // Install ICMPv6 Ping from n0 to n1
  Ipv6Address n1GlobalAddr = iRight.GetAddress (1, 1);
  // Wait for RA to be sent and processed so autoconfig happens
  Ipv6PingHelper ping6 (n1GlobalAddr);
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Size", UintegerValue (56));
  ping6.SetAttribute ("Count", UintegerValue (5));
  ApplicationContainer pingApps = ping6.Install (n0);
  pingApps.Start (Seconds (3.0));
  pingApps.Stop (Seconds (15.0));

  // Enable static routing on the router
  Ptr<Ipv6> ipv6 = r->GetObject<Ipv6> ();
  for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
    ipv6->SetForwarding (i, true);

  // Tracing: Queue length
  Ptr<Queue<Packet>> queueLeft = DynamicCast<CsmaNetDevice>(leftDevices.Get (1))->GetQueue ();
  Simulator::Schedule (Seconds (0.01), &QueueTrace, queueLeft);

  Ptr<Queue<Packet>> queueRight = DynamicCast<CsmaNetDevice>(rightDevices.Get (0))->GetQueue ();
  Simulator::Schedule (Seconds (0.01), &QueueTrace, queueRight);

  // Tracing: Packet reception at n1's right device
  rightDevices.Get (1)->TraceConnectWithoutContext ("MacRx", MakeCallback (&ReceivePacket));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}