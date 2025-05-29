#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngTopologyExample");

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable Log
  LogComponentEnable ("RipngTopologyExample", LOG_LEVEL_INFO);
  LogComponentEnable ("RipNg", LOG_LEVEL_INFO);

  // Create the nodes:  SRC, A, B, C, D, DST
  NodeContainer nodes;
  nodes.Create (6);

  Ptr<Node> src = nodes.Get(0);  // SRC
  Ptr<Node> a   = nodes.Get(1);  // A
  Ptr<Node> b   = nodes.Get(2);  // B
  Ptr<Node> c   = nodes.Get(3);  // C
  Ptr<Node> d   = nodes.Get(4);  // D
  Ptr<Node> dst = nodes.Get(5);  // DST

  // Define all links with point-to-point helpers
  PointToPointHelper p2pDefault;
  p2pDefault.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pDefault.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper p2pCost10;
  p2pCost10.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pCost10.SetChannelAttribute ("Delay", StringValue ("20ms")); // Higher delay simulates higher cost

  // Links: SRC-A, A-B, A-C, B-C, B-D, D-DST, C-D
  NetDeviceContainer dSrcA = p2pDefault.Install (src, a);
  NetDeviceContainer dAB   = p2pDefault.Install (a, b);
  NetDeviceContainer dAC   = p2pDefault.Install (a, c);
  NetDeviceContainer dBC   = p2pDefault.Install (b, c);
  NetDeviceContainer dBD   = p2pDefault.Install (b, d);
  NetDeviceContainer dDDst = p2pDefault.Install (d, dst);
  NetDeviceContainer dCD   = p2pCost10.Install (c, d);

  // Enable Routing
  RipNgHelper ripng;
  ripng.Set ("SplitHorizon", EnumValue (RipNgHelper::POISON_REVERSE));

  // Assign router nodes (A, B, C, D)
  NodeContainer routers;
  routers.Add (a);
  routers.Add (b);
  routers.Add (c);
  routers.Add (d);

  // Install Internet stack on all
  InternetStackHelper internet;
  Ipv6ListRoutingHelper listRouting;
  listRouting.Add (ripng, 0);

  // Only routers run RIPng, SRC/DST just static routing
  InternetStackHelper internetRipng;
  internetRipng.SetIpv4StackInstall (false);
  internetRipng.SetRoutingHelper (listRouting);

  InternetStackHelper internetStatic;
  internetStatic.SetIpv4StackInstall (false);

  internetStatic.Install (src);
  internetRipng.Install (routers);
  internetStatic.Install (dst);

  // IPv6 Address Assignment
  Ipv6AddressHelper addr;
  addr.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifSrcA = addr.Assign (dSrcA);
  addr.NewNetwork ();
  Ipv6InterfaceContainer ifAB   = addr.Assign (dAB);
  addr.NewNetwork ();
  Ipv6InterfaceContainer ifAC   = addr.Assign (dAC);
  addr.NewNetwork ();
  Ipv6InterfaceContainer ifBC   = addr.Assign (dBC);
  addr.NewNetwork ();
  Ipv6InterfaceContainer ifBD   = addr.Assign (dBD);
  addr.NewNetwork ();
  Ipv6InterfaceContainer ifDDst = addr.Assign (dDDst);
  addr.NewNetwork ();
  Ipv6InterfaceContainer ifCD   = addr.Assign (dCD);

  // Assign static addresses for A and D routers
  // (not for routing, for management if needed)
  Ipv6Address addressA ("2001:ffff::a");
  Ipv6Address addressD ("2001:ffff::d");
  Ptr<Ipv6> ipv6A = a->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6D = d->GetObject<Ipv6> ();
  int32_t interfaceA = ipv6A->AddInterface (a->GetDevice (0));
  ipv6A->AddAddress (interfaceA, Ipv6InterfaceAddress (addressA, Ipv6Prefix (128)));
  ipv6A->SetUp (interfaceA);

  int32_t interfaceD = ipv6D->AddInterface (d->GetDevice (0));
  ipv6D->AddAddress (interfaceD, Ipv6InterfaceAddress (addressD, Ipv6Prefix (128)));
  ipv6D->SetUp (interfaceD);

  // Set all router interfaces up, except loopback
  for (uint32_t i = 0; i < routers.GetN (); ++i)
    {
      Ptr<Node> node = routers.Get (i);
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      for (uint32_t j = 0; j < ipv6->GetNInterfaces (); ++j)
        {
          if (j != 0)
            {
              ipv6->SetUp (j);
            }
        }
    }

  // Set interfaces up on SRC and DST as well
  Ptr<Ipv6> ipv6Src = src->GetObject<Ipv6> ();
  for (uint32_t i = 0; i < ipv6Src->GetNInterfaces (); ++i)
    {
      if (i != 0)
        ipv6Src->SetUp (i);
    }
  Ptr<Ipv6> ipv6Dst = dst->GetObject<Ipv6> ();
  for (uint32_t i = 0; i < ipv6Dst->GetNInterfaces (); ++i)
    {
      if (i != 0)
        ipv6Dst->SetUp (i);
    }

  // Configure static routes on SRC and DST
  Ipv6StaticRoutingHelper staticRoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingSrc = staticRoutingHelper.GetStaticRouting (ipv6Src);
  staticRoutingSrc->SetDefaultRoute (ifSrcA.GetAddress (1, 1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingDst = staticRoutingHelper.GetStaticRouting (ipv6Dst);
  staticRoutingDst->SetDefaultRoute (ifDDst.GetAddress (0, 1), 1);

  // Start routing protocol after 3 seconds
  ripng.Set ("RandomJitter", TimeValue (Seconds (0.0)));
  Simulator::Schedule (Seconds (3.0), &RipNg::SetStartTime, StaticCast<RipNg> (a->GetObject<Ipv6RoutingProtocol> ()), Seconds (0.0));
  Simulator::Schedule (Seconds (3.0), &RipNg::SetStartTime, StaticCast<RipNg> (b->GetObject<Ipv6RoutingProtocol> ()), Seconds (0.0));
  Simulator::Schedule (Seconds (3.0), &RipNg::SetStartTime, StaticCast<RipNg> (c->GetObject<Ipv6RoutingProtocol> ()), Seconds (0.0));
  Simulator::Schedule (Seconds (3.0), &RipNg::SetStartTime, StaticCast<RipNg> (d->GetObject<Ipv6RoutingProtocol> ()), Seconds (0.0));
  
  // Install Ping6 on SRC to DST
  uint32_t packetSize = 56;
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds (2.0);

  Ping6Helper ping6;
  ping6.SetLocal (ifSrcA.GetAddress (0, 1));
  ping6.SetRemote (ifDDst.GetAddress (1, 1));
  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer apps = ping6.Install (src);
  apps.Start (Seconds (5.0));
  apps.Stop (Seconds (80.0));

  // Break B-D link at 40 seconds
  Simulator::Schedule (Seconds (40.0), [&]()
  {
    NS_LOG_INFO ("Link between B and D goes down at 40s");
    dBD.Get (0)->SetDown ();
    dBD.Get (1)->SetDown ();
  });

  // Recover B-D link at 44 seconds
  Simulator::Schedule (Seconds (44.0), [&]()
  {
    NS_LOG_INFO ("Link between B and D comes up at 44s");
    dBD.Get (0)->SetUp ();
    dBD.Get (1)->SetUp ();
  });

  // Pcap and ASCII tracing
  AsciiTraceHelper ascii;
  p2pDefault.EnableAsciiAll (ascii.CreateFileStream ("ripng-topo.tr"));
  p2pDefault.EnablePcapAll ("ripng-topo");
  p2pCost10.EnablePcapAll ("ripng-topo-cost10");

  // Run simulation
  Simulator::Stop (Seconds (80.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}