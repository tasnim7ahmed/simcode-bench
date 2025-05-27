#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdTwoPrefix");

void QueueSize(std::string context, Ptr<Queue<Packet>> queue)
{
  NS_LOG_UNCOND (context << "  queueSize: " << queue->GetSize().GetValue ());
}

void PacketReceived(std::string context, Ptr<const Packet> packet, const Address& address)
{
  NS_LOG_UNCOND (context << " Received packet size " << packet->GetSize () << " at " << address);
}

int main (int argc, char *argv[])
{
  LogComponent::Enable ("RadvdTwoPrefix", LOG_LEVEL_INFO);

  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponent::Enable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponent::Enable ("Icmpv6EchoClient", LOG_LEVEL_ALL);
      LogComponent::Enable ("Icmpv6EchoServer", LOG_LEVEL_ALL);
      LogComponent::Enable ("Radvd", LOG_LEVEL_ALL);
    }

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  CsmaHelper csma0;
  csma0.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma0.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  NetDeviceContainer devices0 = csma0.Install (NodeContainer (router.Get (0), nodes.Get (0)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  NetDeviceContainer devices1 = csma1.Install (NodeContainer (router.Get (0), nodes.Get (1)));

  InternetStackHelper internet;
  internet.Install (nodes);
  internet.Install (router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces0 = ipv6.Assign (devices0);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces1 = ipv6.Assign (devices1);

  Ptr<Node> radvdServer = router.Get (0);
  Ptr<Node> node0 = nodes.Get (0);
  Ptr<Node> node1 = nodes.Get (1);

  Ipv6StaticRoutingHelper staticRouting;
  Ptr<Ipv6StaticRouting> routing = staticRouting.GetOrCreateRouting (radvdServer->GetObject<Ipv6> ());
  routing->AddRoutingTableEntry (Ipv6Address ("2001:1::"), 64, interfaces0.GetAddress (0, 1), 1, false);
  routing->AddRoutingTableEntry (Ipv6Address ("2001:2::"), 64, interfaces1.GetAddress (0, 1), 1, false);

  routing = staticRouting.GetOrCreateRouting (node0->GetObject<Ipv6> ());
  routing->AddDefaultRoute (interfaces0.GetAddress (0, 0), 1);

  routing = staticRouting.GetOrCreateRouting (node1->GetObject<Ipv6> ());
  routing->AddDefaultRoute (interfaces1.GetAddress (0, 0), 1);

  Ptr<Radvd> radvd = CreateObject<Radvd> ();
  radvd->SetAdvSendAdvert(true);
  radvd->SetAdvManagedFlag(false);
  radvd->SetAdvOtherConfigFlag(false);

  Ptr<RadvdInterface> radvdInterface0 = radvd->AddInterface (interfaces0.GetAddress (0, 0), devices0.Get (0)->GetIfIndex ());
  radvdInterface0->AddPrefix (Ipv6Prefix ("2001:1::/64"), RadvdInterface::InfiniteLifeTime, RadvdInterface::InfiniteLifeTime, true, false);
  radvdInterface0->AddPrefix (Ipv6Prefix ("2001:ABCD::/64"), RadvdInterface::InfiniteLifeTime, RadvdInterface::InfiniteLifeTime, true, false);

  Ptr<RadvdInterface> radvdInterface1 = radvd->AddInterface (interfaces1.GetAddress (0, 0), devices1.Get (0)->GetIfIndex ());
  radvdInterface1->AddPrefix (Ipv6Prefix ("2001:2::/64"), RadvdInterface::InfiniteLifeTime, RadvdInterface::InfiniteLifeTime, true, false);

  radvdServer->AddApplication (radvd);
  radvd->SetStartTime (Seconds (0.1));
  radvd->SetStopTime (Seconds (10.0));

  uint32_t packetSize = 1024;
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds (1.0);

  Icmpv6EchoClientHelper client (interfaces1.GetAddress (0, 1), 1);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (node0);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Icmpv6EchoServerHelper server;
  ApplicationContainer serverApps = server.Install (node1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Schedule (Seconds (1.0), []() {
    NS_LOG_UNCOND ("Starting Simulation");
  });

  if (tracing)
    {
      std::ofstream ascii;
      ascii.open ("radvd-two-prefix.txt");
      AsciiTraceHelper asciiTraceHelper;
      csma0.EnableAsciiAll (asciiTraceHelper.CreateFileStream ("radvd-two-prefix.tr"));

      devices0.Get (0)->TraceConnectWithoutContext ("PhyTxQueue/Size", MakeCallback (&QueueSize));
      devices0.Get (1)->TraceConnectWithoutContext ("PhyTxQueue/Size", MakeCallback (&QueueSize));
      devices1.Get (0)->TraceConnectWithoutContext ("PhyTxQueue/Size", MakeCallback (&QueueSize));
      devices1.Get (1)->TraceConnectWithoutContext ("PhyTxQueue/Size", MakeCallback (&QueueSize));

      devices0.Get (0)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&PacketReceived));
      devices0.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&PacketReceived));
      devices1.Get (0)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&PacketReceived));
      devices1.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&PacketReceived));
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}