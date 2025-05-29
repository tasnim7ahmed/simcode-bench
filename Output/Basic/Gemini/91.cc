#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceRouting");

uint32_t receivedAtDest = 0;
uint32_t receivedAtSrc = 0;

void ReceivePacketDest (Ptr<Socket> socket)
{
  Address localAddress;
  socket->GetSockName(localAddress);
  while (socket->Recv ())
    {
      receivedAtDest++;
      NS_LOG_INFO ("Received one packet at destination" << InetSocketAddress::ConvertFrom(localAddress).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(localAddress).GetPort() << " total received " << receivedAtDest);
    }
}

void ReceivePacketSrc (Ptr<Socket> socket)
{
  Address localAddress;
  socket->GetSockName(localAddress);
  while (socket->Recv ())
    {
      receivedAtSrc++;
      NS_LOG_INFO ("Received one packet at source loopback" << InetSocketAddress::ConvertFrom(localAddress).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(localAddress).GetPort() << " total received " << receivedAtSrc);
    }
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LogComponent::GetComponent ("MultiInterfaceRouting"), LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (5);

  NodeContainer srcRouter = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer destRouter = NodeContainer (nodes.Get (2), nodes.Get (4));
  NodeContainer routers = NodeContainer (nodes.Get (1), nodes.Get (2));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer srcRouterDevices = p2p.Install (srcRouter);
  NetDeviceContainer destRouterDevices = p2p.Install (destRouter);
  NetDeviceContainer routerDevices = p2p.Install (routers);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer srcRouterInterfaces = ipv4.Assign (srcRouterDevices);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = ipv4.Assign (routerDevices);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer destRouterInterfaces = ipv4.Assign (destRouterDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Static routing configuration
  Ptr<Ipv4StaticRouting> staticRoutingSource = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> ());
  staticRoutingSource->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.1.2"), 1);

  Ptr<Ipv4StaticRouting> staticRoutingRouter1 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  staticRoutingRouter1->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.2.2"), 1);

  Ptr<Ipv4StaticRouting> staticRoutingRouter2 = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (2)->GetObject<Ipv4> ());
  staticRoutingRouter2->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.2.1"), 1);

  Ptr<Ipv4StaticRouting> staticRoutingDest = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (4)->GetObject<Ipv4> ());
  staticRoutingDest->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.1"), 1);

  // UDP client server setup
  uint16_t port = 9;
  UdpClientHelper client (destRouterInterfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Bind sockets to different interfaces and set receive callbacks
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetLoopback (), port);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacketSrc));

  Ptr<Socket> recvSinkDest = Socket::CreateSocket (nodes.Get (4), tid);
  InetSocketAddress localDest = InetSocketAddress (destRouterInterfaces.GetAddress (1), port);
  recvSinkDest->Bind (localDest);
  recvSinkDest->SetRecvCallback (MakeCallback (&ReceivePacketDest));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}