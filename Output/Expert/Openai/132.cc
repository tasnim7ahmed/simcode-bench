#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DvTwoNodeLoop");

void PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRt = staticRouting.GetStaticRouting (ipv4);
  NS_LOG_INFO ("[" << Simulator::Now ().GetSeconds () << "s] Routing table of node " << node->GetId ());
  staticRt->PrintRoutingTable (Create<OutputStreamWrapper> (&std::cout));
}

void GenerateTraffic (Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
  if (pktCount > 0)
    {
      Ptr<Packet> pkt = Create<Packet> (pktSize);
      socket->SendTo (pkt, 0, InetSocketAddress (dstAddr, port));
      Simulator::Schedule (pktInterval, &GenerateTraffic, socket, dstAddr, port, pktSize, pktCount - 1, pktInterval);
    }
}

void LoopPacketSink (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      // Do nothing, just drain packets
    }
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("DvTwoNodeLoop", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv4StaticRouting", LOG_LEVEL_WARN);

  NodeContainer nodes;
  nodes.Create (3); // 0: A, 1: B, 2: D (Destination)

  // Create point-to-point links: A<--->B, B<--->D, A<--->D
  NodeContainer ab = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer bd = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer ad = NodeContainer (nodes.Get (0), nodes.Get (2));
  
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer abDevices = p2p.Install (ab);
  NetDeviceContainer bdDevices = p2p.Install (bd);
  NetDeviceContainer adDevices = p2p.Install (ad);

  // Assign IP addresses
  InternetStackHelper internet;
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4ListRoutingHelper listRouting;
  internet.SetRoutingHelper (listRouting);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer abIf = ipv4.Assign (abDevices);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bdIf = ipv4.Assign (bdDevices);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer adIf = ipv4.Assign (adDevices);

  // Setup static routes to simulate DV
  // Node 0 (A): route to D via direct and via B
  Ptr<Ipv4> aIpv4 = nodes.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> aRoute = staticRouting.GetStaticRouting (aIpv4);
  aRoute->AddHostRouteTo (adIf.GetAddress (1), adIf.GetAddress (1), 2); // via direct link
  aRoute->AddHostRouteTo (bdIf.GetAddress (1), abIf.GetAddress (1), 1); // via B

  // Node 1 (B): route to D via direct and via A
  Ptr<Ipv4> bIpv4 = nodes.Get (1)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> bRoute = staticRouting.GetStaticRouting (bIpv4);
  bRoute->AddHostRouteTo (bdIf.GetAddress (1), bdIf.GetAddress (1), 2); // via direct link
  bRoute->AddHostRouteTo (adIf.GetAddress (1), abIf.GetAddress (0), 1); // via A

  // Node 2 (D): no special routes needed

  // Setup apps: Node 0 (A) generates traffic to Node 2 (D)
  uint16_t dstPort = 5000;
  Ptr<Socket> sinkSocket = Socket::CreateSocket (nodes.Get (2), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), dstPort);
  sinkSocket->Bind (local);
  sinkSocket->SetRecvCallback (MakeCallback (&LoopPacketSink));

  Ptr<Socket> srcSocket = Socket::CreateSocket (nodes.Get (0), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  Simulator::ScheduleWithContext (srcSocket->GetNode ()->GetId (), Seconds (1.0),
    &GenerateTraffic, srcSocket, adIf.GetAddress (1), dstPort, 512, 20, Seconds (0.5));

  // Print routing tables before failure for debugging
  Simulator::Schedule (Seconds (0.5), &PrintRoutingTable, nodes.Get (0), Seconds (0.5));
  Simulator::Schedule (Seconds (0.5), &PrintRoutingTable, nodes.Get (1), Seconds (0.5));

  // After 3s, bring down direct links to D to simulate failure
  Simulator::Schedule (Seconds (3.0), [&] {
      NS_LOG_INFO ("Simulating direct link failure to D");
      adDevices.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (CreateObject<RateErrorModel> ()));
      adDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (CreateObject<RateErrorModel> ()));
      bdDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (CreateObject<RateErrorModel> ()));
      bdDevices.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (CreateObject<RateErrorModel> ()));
      // Now A and B re route packets for D to each other (manually manipulate static routes as if they have outdated information)
      // Remove direct routes to D
      Ptr<Ipv4StaticRouting> aRt = staticRouting.GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> ());
      aRt->RemoveRoute (0); // Assuming direct route is index 0
      Ptr<Ipv4StaticRouting> bRt = staticRouting.GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
      bRt->RemoveRoute (0); // Assuming direct route is index 0
  });

  // Print debug routing tables after the failure
  Simulator::Schedule (Seconds (3.5), &PrintRoutingTable, nodes.Get (0), Seconds (3.5));
  Simulator::Schedule (Seconds (3.5), &PrintRoutingTable, nodes.Get (1), Seconds (3.5));

  // Run simulation for enough time to observe looping
  Simulator::Stop (Seconds (8.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}