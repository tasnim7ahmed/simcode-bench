#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/trace-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationExample");

void
RxTrace (std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream rxStream ("fragmentation-ipv6.tr", std::ios_base::app);
  rxStream << "RX " << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << " bytes from " << Inet6SocketAddress::ConvertFrom(address).GetIpv6 () << std::endl;
}

void
EnqueueTrace (std::string context, Ptr<const Packet> packet)
{
  static std::ofstream queueStream ("fragmentation-ipv6.tr", std::ios_base::app);
  queueStream << "ENQUEUE " << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << " bytes\n";
}

void
DequeueTrace (std::string context, Ptr<const Packet> packet)
{
  static std::ofstream queueStream ("fragmentation-ipv6.tr", std::ios_base::app);
  queueStream << "DEQUEUE " << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << " bytes\n";
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv6FragmentationExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer n0n1r;
  n0n1r.Create (3);
  Ptr<Node> n0 = n0n1r.Get (0);
  Ptr<Node> r  = n0n1r.Get (1);
  Ptr<Node> n1 = n0n1r.Get (2);

  // CSMA channel parameters
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create left and right nets (n0<->r and r<->n1)
  NodeContainer left (n0, r);
  NodeContainer right (r, n1);

  NetDeviceContainer leftDevices = csma.Install (left);
  NetDeviceContainer rightDevices = csma.Install (right);

  // Internet stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (n0n1r);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifaceLeft = ipv6.Assign (leftDevices);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifaceRight = ipv6.Assign (rightDevices);

  // Set up forwarding on the router
  Ptr<Ipv6> ipv6r = r->GetObject<Ipv6> ();
  for (uint32_t i = 0; i < ipv6r->GetNInterfaces (); ++i)
    {
      ipv6r->SetForwarding (i, true);
      ipv6r->SetDefaultRoute (ipv6r->GetAddress (i, 1).GetAddress (), i);
    }

  // Add default routes for end nodes
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  staticRoutingN0->SetDefaultRoute (ifaceLeft.GetAddress (1, 1), 1);
  staticRoutingN1->SetDefaultRoute (ifaceRight.GetAddress (0, 1), 1);

  // Assign link-local addresses
  ifaceLeft.SetForwarding (1, true);
  ifaceLeft.SetDefaultRouteInAllNodes (1);

  ifaceRight.SetForwarding (0, true);
  ifaceRight.SetDefaultRouteInAllNodes (0);

  // UDP echo server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP echo client on n0, sending large packets to trigger IPv6 fragmentation
  uint32_t packetSize = 2000; // Large enough for fragmentation
  uint32_t maxPackets = 3;
  UdpEchoClientHelper echoClient (ifaceRight.GetAddress (1, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (n0);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable queue and receive event tracing
  Config::Connect ("/NodeList/2/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback (&RxTrace));
  // Queue events (for both CSMA channels)
  for (uint32_t d = 0; d < leftDevices.GetN (); ++d)
    {
      std::ostringstream ossEnq, ossDeq;
      ossEnq << "/NodeList/" << left.Get(d)->GetId ()
             << "/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/Enqueue";
      ossDeq << "/NodeList/" << left.Get(d)->GetId ()
             << "/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/Dequeue";
      Config::Connect (ossEnq.str (), MakeCallback (&EnqueueTrace));
      Config::Connect (ossDeq.str (), MakeCallback (&DequeueTrace));
    }
  for (uint32_t d = 0; d < rightDevices.GetN (); ++d)
    {
      std::ostringstream ossEnq, ossDeq;
      ossEnq << "/NodeList/" << right.Get(d)->GetId ()
             << "/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/Enqueue";
      ossDeq << "/NodeList/" << right.Get(d)->GetId ()
             << "/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/Dequeue";
      Config::Connect (ossEnq.str (), MakeCallback (&EnqueueTrace));
      Config::Connect (ossDeq.str (), MakeCallback (&DequeueTrace));
    }

  // Enable pcap tracing
  csma.EnablePcapAll ("fragmentation-ipv6", false);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}