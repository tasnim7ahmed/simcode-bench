#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/ipv6-ping-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

void
RxTrace (std::string context, Ptr<const Packet> p, const Address &a)
{
  std::cout << Simulator::Now ().GetSeconds () << "s: Packet received. Context: " << context << ", Packet size: " << p->GetSize () << std::endl;
}

void
QueueTrace (std::string context, Ptr<const QueueDiscItem> item)
{
  std::cout << Simulator::Now ().GetSeconds () << "s: Packet enqueued at DropTailQueue. Context: " << context << ", Packet size: " << item->GetPacket ()->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_INFO);
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnableAll (LOG_PREFIX_NODE);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  // IEEE 802.15.4 LR-WPAN setup
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devs = lrWpanHelper.Install (nodes);

  lrWpanHelper.AssociateToPan (devs, 0);

  // Install DropTail queues
  for (uint32_t i = 0; i < devs.GetN (); ++i)
    {
      Ptr<Queue<Packet>> queue = CreateObjectWithAttributes<DropTailQueue<Packet>> (
          "MaxSize", QueueSizeValue (QueueSize ("100p")));
      devs.Get (i)->SetAttribute ("TxQueue", PointerValue (queue));
    }

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack (IPv6 only)
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));

  Ipv6InterfaceContainer interfaces = ipv6.Assign (devs);
  interfaces.SetForwarding (0, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  // Trace: queue events
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("wsn-ping6.tr");

  // Trace rx at both devices
  devs.Get (0)->TraceConnect("MacRx", "", MakeCallback(&RxTrace));
  devs.Get (1)->TraceConnect("MacRx", "", MakeCallback(&RxTrace));

  // Trace DropTail queues
  for (uint32_t i = 0; i < devs.GetN (); ++i)
    {
      Ptr<Queue<Packet>> txQueue = devs.Get (i)->GetObject<Queue<Packet>> ();
      if (txQueue)
        {
          txQueue->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&QueueTrace));
          txQueue->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback ([stream] (std::string context, Ptr<const QueueDiscItem> item)
          {
            *stream->GetStream () << Simulator::Now ().GetSeconds () << " QUEUE ENQUEUE " << item->GetPacket ()->GetSize () << "B\n";
          }));
        }
    }

  // Ping6 Setup: node 0 pings node 1
  Ipv6PingHelper ping (interfaces.GetAddress (1, 1));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ping.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping.SetAttribute ("Size", UintegerValue (64));
  ping.SetAttribute ("Count", UintegerValue (5));
  ApplicationContainer apps = ping.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (12.0));

  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}