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

void ReceivePacketSource (Ptr<Socket> socket)
{
  Address localAddress;
  socket->GetSockName(localAddress);
  while (socket->Recv ())
  {
    NS_LOG_UNCOND ("S Source received one packet from " << InetSocketAddress::ConvertFrom(localAddress).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(localAddress).GetPort());
  }
}

void ReceivePacketDestination (Ptr<Socket> socket)
{
  Address localAddress;
  socket->GetSockName(localAddress);
  while (socket->Recv ())
  {
    NS_LOG_UNCOND ("D Destination received one packet from " << InetSocketAddress::ConvertFrom(localAddress).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(localAddress).GetPort());
  }
}

static void
BindSock (Ptr<Node> node, Ipv4Address address, uint16_t port)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sock = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (address, port);
  sock->Bind (local);
  sock->SetRecvCallback (MakeCallback (&ReceivePacketSource));
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);

  NodeContainer sourceRouter = NodeContainer (nodes.Get (0), nodes.Get (2));
  NodeContainer destRouter = NodeContainer (nodes.Get (1), nodes.Get (3));
  NodeContainer left = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer route1 = NodeContainer (nodes.Get (2), nodes.Get (4));
  NodeContainer route2 = NodeContainer (nodes.Get (3), nodes.Get (5));
  NodeContainer right = NodeContainer (nodes.Get (4), nodes.Get (5));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer sourceRouterDevices = pointToPoint.Install (sourceRouter);
  NetDeviceContainer destRouterDevices = pointToPoint.Install (destRouter);
  NetDeviceContainer leftDevices = pointToPoint.Install (left);
  NetDeviceContainer route1Devices = pointToPoint.Install (route1);
  NetDeviceContainer route2Devices = pointToPoint.Install (route2);
  NetDeviceContainer rightDevices = pointToPoint.Install (right);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sourceRouterInterfaces = address.Assign (sourceRouterDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer destRouterInterfaces = address.Assign (destRouterDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces = address.Assign (leftDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer route1Interfaces = address.Assign (route1Devices);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer route2Interfaces = address.Assign (route2Devices);

  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer rightInterfaces = address.Assign (rightDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Ipv4StaticRouting> staticRouting1 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> ());
  staticRouting1->AddHostRouteToNetwork (Ipv4Address ("10.1.6.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.2"), 1);
  staticRouting1->AddHostRouteToNetwork (Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.2"), 1);
  staticRouting1->AddHostRouteToNetwork (Ipv4Address ("10.1.5.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.2"), 1);

  Ptr<Ipv4StaticRouting> staticRouting2 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  staticRouting2->AddHostRouteToNetwork (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.1"), 1);
  staticRouting2->AddHostRouteToNetwork (Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.1"), 1);
  staticRouting2->AddHostRouteToNetwork (Ipv4Address ("10.1.5.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.1"), 1);

  Ptr<Ipv4StaticRouting> staticRouting3 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (2)->GetObject<Ipv4> ());
  staticRouting3->AddNetworkRouteTo (Ipv4Address ("10.1.6.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.4.1"), 1);
  staticRouting3->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.1.2"), 2);

  Ptr<Ipv4StaticRouting> staticRouting4 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (3)->GetObject<Ipv4> ());
  staticRouting4->AddNetworkRouteTo (Ipv4Address ("10.1.6.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.5.1"), 2);
  staticRouting4->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.2.2"), 1);

  Ptr<Ipv4StaticRouting> staticRouting5 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (4)->GetObject<Ipv4> ());
  staticRouting5->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.6.1"), 1);
  staticRouting5->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.6.1"), 1);
  staticRouting5->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.6.1"), 1);

  Ptr<Ipv4StaticRouting> staticRouting6 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (5)->GetObject<Ipv4> ());
  staticRouting6->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.6.2"), 1);
  staticRouting6->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.6.2"), 1);
  staticRouting6->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.6.2"), 1);

  UdpClientServerHelper udpClientServer (9, Ipv4Address("10.1.6.2"), 5000);
  udpClientServer.SetAttribute ("MaxPackets", UintegerValue (100));
  udpClientServer.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  udpClientServer.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = udpClientServer.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 5000);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacketDestination));

  BindSock (nodes.Get (0), Ipv4Address("10.1.1.1"), 5001);
  BindSock (nodes.Get (0), Ipv4Address("10.1.3.1"), 5002);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}