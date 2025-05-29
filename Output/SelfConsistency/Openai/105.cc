#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationExample");

// Trace callback for queue events.
void QueueTrace (std::string context, Ptr<const Packet> packet)
{
  static std::ofstream out ("fragmentation-ipv6.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds () << "s QUEUE: " << context
      << " Packet UID: " << packet->GetUid () << " Size: " << packet->GetSize () << std::endl;
}

// Trace callback for packet reception
void RxTrace (std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out ("fragmentation-ipv6.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds () << "s RX: " << context
      << " Packet UID: " << packet->GetUid () << " Size: " << packet->GetSize () << std::endl;
}

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("Ipv6FragmentationExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes: n0, r, n1
  NodeContainer nodes;
  nodes.Create (3);
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> r  = nodes.Get (1);
  Ptr<Node> n1 = nodes.Get (2);

  // Create CSMA links
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Network 1: n0 <-> r
  NodeContainer net1 (n0, r);
  NetDeviceContainer devs1 = csmaHelper.Install (net1);

  // Network 2: r <-> n1
  NodeContainer net2 (r, n1);
  NetDeviceContainer devs2 = csmaHelper.Install (net2);

  // Internet stack (IPv6)
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6_1;
  ipv6_1.SetBase ("2001:1::", 64);
  Ipv6InterfaceContainer interfaces1 = ipv6_1.Assign (devs1);

  Ipv6AddressHelper ipv6_2;
  ipv6_2.SetBase ("2001:2::", 64);
  Ipv6InterfaceContainer interfaces2 = ipv6_2.Assign (devs2);

  interfaces1.SetForwarding (1, true); // router interface on net1
  interfaces2.SetForwarding (0, true); // router interface on net2

  // Set default routes on n0 and n1 to the router
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> n0StaticRouting = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  n0StaticRouting->SetDefaultRoute (interfaces1.GetAddress (1, 1), 1);

  Ptr<Ipv6StaticRouting> n1StaticRouting = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  n1StaticRouting->SetDefaultRoute (interfaces2.GetAddress (0, 1), 1);

  // Set up UDP Echo server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create large packet to force fragmentation: IPv6 minimum MTU is 1280, so pick e.g. 3000 bytes payload
  uint32_t packetSize = 3000;

  // Set up UDP Echo client on n0, targetting n1's IPv6 address
  UdpEchoClientHelper echoClient (interfaces2.GetAddress (1, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (n0);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing: connect queue and receive events to file
  // Net1 devices
  devs1.Get (0)->TraceConnect ("Enqueue", "n0->r", MakeCallback (&QueueTrace));
  devs1.Get (1)->TraceConnect ("Enqueue", "r<->n0", MakeCallback (&QueueTrace));
  devs1.Get (0)->TraceConnect ("PhyRxEnd", "n0", MakeBoundCallback (&RxTrace));
  devs1.Get (1)->TraceConnect ("PhyRxEnd", "r(net1)", MakeBoundCallback (&RxTrace));

  // Net2 devices
  devs2.Get (0)->TraceConnect ("Enqueue", "r<->n1", MakeCallback (&QueueTrace));
  devs2.Get (1)->TraceConnect ("Enqueue", "n1->r", MakeCallback (&QueueTrace));
  devs2.Get (0)->TraceConnect ("PhyRxEnd", "r(net2)", MakeBoundCallback (&RxTrace));
  devs2.Get (1)->TraceConnect ("PhyRxEnd", "n1", MakeBoundCallback (&RxTrace));

  // Enable pcap tracing
  csmaHelper.EnablePcapAll ("fragmentation-ipv6", false);

  // Run simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}