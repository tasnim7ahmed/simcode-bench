#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

void
SendIcmpv6Redirect (Ptr<Node> r1, Ipv6Address sta1Addr, Ipv6Address sta2Addr, Ipv6Address r2Addr)
{
  // Manually craft and send ICMPv6 Redirect from R1 to STA1
  // Ipv6L3Protocol generates redirect if proper conditions, but we do it here explicitly

  Ptr<Icmpv6L4Protocol> icmp6 = r1->GetObject<Icmpv6L4Protocol>();
  // Find netdevice index to STA1
  Ptr<Ipv6> ipv6 = r1->GetObject<Ipv6>();
  int interfaceToSta1 = -1;
  int interfaceToR2 = -1;
  for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
    {
      for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
        {
          Ipv6Address addr = ipv6->GetAddress(i, j).GetAddress();
          if (addr.IsLinkLocal() || addr.IsLoopback())
            continue;
          if (addr.CombinePrefix(64) == Ipv6Address("2001:db8:1::"))
            interfaceToSta1 = i;
          if (addr.CombinePrefix(64) == Ipv6Address("2001:db8:2::"))
            interfaceToR2 = i;
        }
    }
  NS_ASSERT(interfaceToSta1 >= 0 && interfaceToR2 >= 0);

  // Send IS_ICMP6_REDIRECT from R1 to STA1: "to reach STA2, use R2's address"
  Ptr<Packet> emptyPayload = Create<Packet> (); // Echo has already finished – just trigger mechanism
  icmp6->SendRedirect (sta1Addr, sta2Addr, r2Addr, interfaceToSta1, emptyPayload);
}


static void
Icmpv6Tracer (std::string context, Ptr<const Packet> packet, Ipv6Address src, Ipv6Address dst, uint8_t type, uint8_t code, uint32_t ifIndex)
{
  static std::ofstream trf ("icmpv6-redirect.tr", std::ios_base::app);
  trf << Simulator::Now ().GetSeconds ()
      << " " << context
      << " src=" << src
      << " dst=" << dst
      << " type=" << unsigned(type)
      << " code=" << unsigned(code)
      << " ifIndex=" << ifIndex
      << std::endl;
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer staNodes, routerNodes;
  staNodes.Create (2); // STA1 (0), STA2 (1)
  routerNodes.Create (2); // R1 (0), R2 (1)

  // Layout:
  //    STA1 <---P2P1---> R1 <---P2P2---> R2 <---P2P3---> STA2
  NodeContainer p2p1 (staNodes.Get(0), routerNodes.Get(0));
  NodeContainer p2p2 (routerNodes.Get(0), routerNodes.Get(1));
  NodeContainer p2p3 (routerNodes.Get(1), staNodes.Get(1));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devSta1R1 = p2p.Install (p2p1);
  NetDeviceContainer devR1R2 = p2p.Install (p2p2);
  NetDeviceContainer devR2Sta2 = p2p.Install (p2p3);

  // Install IPv6 stack
  InternetStackHelper internetv6;
  internetv6.Install (staNodes);
  internetv6.Install (routerNodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;

  // STA1 <--> R1
  ipv6.SetBase ("2001:db8:1::", 64);
  Ipv6InterfaceContainer ifSta1 = ipv6.Assign (devSta1R1);
  ifSta1.SetForwarding (1, true); // R1 forwards
  ifSta1.SetDefaultRouteInAllNodes (0); // Set default route for STA1

  // R1 <--> R2
  ipv6.SetBase ("2001:db8:2::", 64);
  Ipv6InterfaceContainer ifR1R2 = ipv6.Assign (devR1R2);
  ifR1R2.SetForwarding (0, true); // R1
  ifR1R2.SetForwarding (1, true); // R2

  // R2 <--> STA2
  ipv6.SetBase ("2001:db8:3::", 64);
  Ipv6InterfaceContainer ifR2Sta2 = ipv6.Assign (devR2Sta2);
  ifR2Sta2.SetForwarding (0, true); // R2
  ifR2Sta2.SetDefaultRouteInAllNodes (1); // Set default for STA2

  // Remove RA egress so we manage routes manually
  Ipv6StaticRoutingHelper staticRoutingHelper;

  // 1. STA1: Default route via R1 already set via SetDefaultRouteInAllNodes (0)
  // 2. R1: Route to STA2 via R2
  Ptr<Ipv6StaticRouting> r1Static = staticRoutingHelper.GetStaticRouting (routerNodes.Get(0)->GetObject<Ipv6>());
  r1Static->SetDefaultRoute (ifSta1.GetAddress(0,1), 1); // Forwarding for transit to enable redirect generation

  r1Static->AddNetworkRouteTo (Ipv6Address("2001:db8:3::"), Ipv6Prefix(64), ifR1R2.GetAddress(0,1), ifR1R2.GetInterfaceIndex(0));

  // 3. R2: Default route to R2 itself not needed; already acts as router
  // 4. STA2: Default route via R2 already set

  // Install Ping6 app from STA1 to STA2
  uint32_t packetSize = 56;
  uint32_t maxPackets = 4;
  Time interPacketInterval = Seconds (1.0);

  Ping6Helper ping6;
  ping6.SetLocal (ifSta1.GetAddress (0, 1));
  ping6.SetRemote (ifR2Sta2.GetAddress (1, 1)); // STA2's address
  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer apps = ping6.Install (staNodes.Get(0)); // STA1
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Tracing: Tap ICMPv6
  Config::Connect ("/NodeList/*/$ns3::Ipv6L3Protocol/TxICMPv6", MakeCallback(&Icmpv6Tracer));
  Config::Connect ("/NodeList/*/$ns3::Ipv6L3Protocol/RxICMPv6", MakeCallback(&Icmpv6Tracer));

  // Schedule explicit Redirect from R1 to STA1 after first echo has transited
  Simulator::Schedule (Seconds (2.5),
      &SendIcmpv6Redirect,
      routerNodes.Get(0),
      ifSta1.GetAddress(0,1),      // STA1's address
      ifR2Sta2.GetAddress(1,1),    // STA2's address
      ifR1R2.GetAddress(0,1));     // Next-hop (R2's address on R1-R2 link)

  Simulator::Stop (Seconds(12.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}