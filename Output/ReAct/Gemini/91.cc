#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/net-device.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceStaticRouting");

uint32_t packetsReceivedSink = 0;
uint32_t packetsReceivedSource = 0;

void ReceivePacketSink (Ptr<Socket> socket)
{
  Address remoteAddr;
  Ptr<Packet> packet = socket->RecvFrom (remoteAddr);

  packetsReceivedSink++;
  NS_LOG_INFO ("Sink Received one packet! Packet Size: " << packet->GetSize () <<
               " from " << InetSocketAddress::ConvertFrom (remoteAddr).GetIpv4 () <<
               " on port " << InetSocketAddress::ConvertFrom (remoteAddr).GetPort ());
}

void ReceivePacketSource (Ptr<Socket> socket)
{
  Address remoteAddr;
  Ptr<Packet> packet = socket->RecvFrom (remoteAddr);

  packetsReceivedSource++;
  NS_LOG_INFO ("Source Received one packet! Packet Size: " << packet->GetSize () <<
               " from " << InetSocketAddress::ConvertFrom (remoteAddr).GetIpv4 () <<
               " on port " << InetSocketAddress::ConvertFrom (remoteAddr).GetPort ());
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("1ms"));

  NodeContainer nodes;
  nodes.Create (6);

  NodeContainer sourceNode = NodeContainer (nodes.Get (0));
  NodeContainer destNode = NodeContainer (nodes.Get (5));
  NodeContainer router1 = NodeContainer (nodes.Get (1));
  NodeContainer router2 = NodeContainer (nodes.Get (4));
  NodeContainer r1r2 = NodeContainer (nodes.Get (2), nodes.Get (3));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = p2p.Install (sourceNode, router1);
  NetDeviceContainer devices02 = p2p.Install (sourceNode, router2);
  NetDeviceContainer devices12 = p2p.Install (router1, r1r2.Get (0));
  NetDeviceContainer devices45 = p2p.Install (router2, r1r2.Get (1));
  NetDeviceContainer devices15 = p2p.Install (r1r2.Get (0), destNode);
  NetDeviceContainer devices46 = p2p.Install (r1r2.Get (1), destNode);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (devices01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign (devices02);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (devices12);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i45 = ipv4.Assign (devices45);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i15 = ipv4.Assign (devices15);

  ipv4.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i46 = ipv4.Assign (devices46);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Ipv4StaticRouting> staticRouting1 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get (1)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting1->SetDefaultRoute ("10.1.3.2", 1);

  Ptr<Ipv4StaticRouting> staticRouting4 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get (4)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting4->SetDefaultRoute ("10.1.4.2", 1);

  Ptr<Ipv4StaticRouting> staticRouting0 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting0->AddHostRoute (Ipv4Address ("10.1.5.2"), Ipv4Address ("10.1.1.2"), 1);
  staticRouting0->AddHostRoute (Ipv4Address ("10.1.6.2"), Ipv4Address ("10.1.2.2"), 2);

  uint16_t port = 9;

  UdpClientServerHelper cS (port);
  cS.SetAttribute ("MaxPackets", UintegerValue (1000));
  cS.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  cS.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  serverApps = cS.Install (destNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  clientApps = cS.Install (sourceNode);
  Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps.Get(0));
  client->SetAttribute("RemoteAddress", AddressValue (InetSocketAddress (i46.GetAddress(0), port)));
  client->SetAttribute("RemotePort", UintegerValue (port));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSocketSink = Socket::CreateSocket (destNode.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocketSink->Bind (local);
  recvSocketSink->SetRecvCallback (MakeCallback (&ReceivePacketSink));

  Ptr<Socket> recvSocketSource1 = Socket::CreateSocket (sourceNode.Get (0), tid);
  InetSocketAddress localSource1 = InetSocketAddress (i01.GetAddress(0), port + 1);
  recvSocketSource1->Bind (localSource1);
  recvSocketSource1->SetRecvCallback (MakeCallback (&ReceivePacketSource));

  Ptr<Socket> recvSocketSource2 = Socket::CreateSocket (sourceNode.Get (0), tid);
  InetSocketAddress localSource2 = InetSocketAddress (i02.GetAddress(0), port + 2);
  recvSocketSource2->Bind (localSource2);
  recvSocketSource2->SetRecvCallback (MakeCallback (&ReceivePacketSource));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();

  NS_LOG_INFO ("Packets received at sink: " << packetsReceivedSink);
  NS_LOG_INFO ("Packets received at source: " << packetsReceivedSource);

  return 0;
}