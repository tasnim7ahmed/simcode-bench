#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/packet.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6Options");

uint32_t g_rxPackets = 0;

static void
RxCallback (Ptr<const Packet> packet, const Address &from)
{
  g_rxPackets++;
  auto tag = packet->FindFirstMatchingByteTag<Packet::Ip6HeaderByteTag> ();

  if (tag)
    {
      Ipv6Header header;
      header.Deserialize (packet->GetHeader (tag->GetStart (), tag->GetEnd ()));

      NS_LOG_INFO ("Received Packet: Source=" << from
                                                << ", Dst=" << header.GetDestinationAddress ()
                                                << ", TCLASS=" << (uint32_t) header.GetTrafficClass ()
                                                << ", HOPLIMIT=" << (uint32_t) header.GetHopLimit ());
    }
  else
    {
      NS_LOG_INFO ("Received Packet without IPv6 Header Info");
    }
}

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("UdpClient", "DataRate", StringValue ("100Mbps"));
  LogComponent::SetAttribute ("UdpClient", "PacketSize", UintegerValue (1024));

  bool enableAscii = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time interPacketInterval = MilliSeconds (10);
  uint8_t tclassValue = 0x00;
  uint8_t hoplimitValue = 64;

  CommandLine cmd;
  cmd.AddValue ("ascii", "Enable ascii tracing", enableAscii);
  cmd.AddValue ("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets", interPacketInterval);
  cmd.AddValue ("tclass", "TCLASS Value", tclassValue);
  cmd.AddValue ("hoplimit", "HOPLIMIT Value", hoplimitValue);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (packetSize));
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (interPacketInterval));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (numPackets));

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  // Set up static routing
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetif (Ipv6Address ("2001:db8:0:1::2"), 0, 1);
  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetif (Ipv6Address ("2001:db8:0:1::1"), 0, 1);

  UdpClientServerHelper udpClientServer (9, 9);
  udpClientServer.SetAttribute ("RemoteAddress", AddressValue (interfaces.GetAddress (1, 1)));
  udpClientServer.SetAttribute ("Tclass", UintegerValue (tclassValue));
  udpClientServer.SetAttribute ("HopLimit", UintegerValue (hoplimitValue));

  ApplicationContainer clientApps = udpClientServer.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Packet::EnableIp6HeaderByteTag (true);

  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Application> app = n1->GetApplication (0);
  Ptr<UdpServer> server = DynamicCast<UdpServer> (app);
  server->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));

  Simulator::Stop (Seconds (11.0));

  if (enableAscii)
    {
      AsciiTraceHelper ascii;
      csma.EnableAsciiAll (ascii.CreateFileStream ("ipv6-options.tr"));
    }

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}