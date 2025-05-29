#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ping6-helper.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/propagation-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/queue.h"
#include "ns3/trace-helper.h"

using namespace ns3;

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("Packet received at " << Simulator::Now ().GetSeconds () << "s Size: " << packet->GetSize ());
}

void
QueueEnqueue (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Enqueued packet at " << Simulator::Now ().GetSeconds () << "s Size: " << packet->GetSize ());
}

void
QueueDequeue (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Dequeued packet at " << Simulator::Now ().GetSeconds () << "s Size: " << packet->GetSize ());
}

int
main(int argc, char *argv[])
{
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6Interface", LOG_LEVEL_INFO);
  LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable ("LrWpanMac", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  Ptr<LrWpanNetDevice> dev0 = CreateObject<LrWpanNetDevice> ();
  Ptr<LrWpanNetDevice> dev1 = CreateObject<LrWpanNetDevice> ();
  nodes.Get(0)->AddDevice (dev0);
  nodes.Get(1)->AddDevice (dev1);

  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel> ();
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
  channel->AddPropagationLossModel (lossModel);
  channel->AddPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
  dev0->SetChannel (channel);
  dev1->SetChannel (channel);

  dev0->GetMac ()->SetAddress (Mac16Address ("00:01"));
  dev1->GetMac ()->SetAddress (Mac16Address ("00:02"));

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devices;
  devices.Add (dev0);
  devices.Add (dev1);
  lrWpanHelper.AssociateToPan (dev0, 0);
  lrWpanHelper.AssociateToPan (dev1, 0);

  PacketSocketHelper packetSocket;
  packetSocket.Install (nodes);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase ("2001:db8::", 64);
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  // Bring up interfaces
  interfaces.SetForwarding (0, true);
  interfaces.SetForwarding (1, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  // Attach trace sources
  Ptr<Queue<Packet> > queue0 = dev0->GetQueue ();
  if (!queue0)
    {
      queue0 = CreateObject<DropTailQueue<Packet> > ();
      dev0->SetQueue (queue0);
    }
  Ptr<Queue<Packet> > queue1 = dev1->GetQueue ();
  if (!queue1)
    {
      queue1 = CreateObject<DropTailQueue<Packet> > ();
      dev1->SetQueue (queue1);
    }

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("wsn-ping6.tr");
  if (queue0)
    {
      queue0->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueue));
      queue0->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&AsciiTraceHelper::DefaultEnqueueSinkWithContext, stream));
      queue0->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeue));
      queue0->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&AsciiTraceHelper::DefaultDequeueSinkWithContext, stream));
    }
  if (queue1)
    {
      queue1->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueue));
      queue1->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&AsciiTraceHelper::DefaultEnqueueSinkWithContext, stream));
      queue1->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeue));
      queue1->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&AsciiTraceHelper::DefaultDequeueSinkWithContext, stream));
    }

  // Trace packet receptions
  dev0->TraceConnectWithoutContext ("MacRx", MakeCallback (&RxTrace));
  dev1->TraceConnectWithoutContext ("MacRx", MakeCallback (&RxTrace));

  // ICMPv6 Echo request from node 0 to node 1
  Ping6Helper ping6;
  ping6.SetLocal (interfaces.GetAddress (0, 1));
  ping6.SetRemote (interfaces.GetAddress (1, 1));
  ping6.SetAttribute ("MaxPackets", UintegerValue (5));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("PacketSize", UintegerValue (56));
  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (15.0));

  // ICMPv6 Echo request from node 1 to node 0
  Ping6Helper ping6_rev;
  ping6_rev.SetLocal (interfaces.GetAddress (1, 1));
  ping6_rev.SetRemote (interfaces.GetAddress (0, 1));
  ping6_rev.SetAttribute ("MaxPackets", UintegerValue (5));
  ping6_rev.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6_rev.SetAttribute ("PacketSize", UintegerValue (56));
  ApplicationContainer apps_rev = ping6_rev.Install (nodes.Get (1));
  apps_rev.Start (Seconds (4.0));
  apps_rev.Stop (Seconds (15.0));

  Simulator::Stop (Seconds (16.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}