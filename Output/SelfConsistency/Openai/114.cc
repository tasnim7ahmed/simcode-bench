/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Wireless Sensor Network (WSN) 2-node IEEE 802.15.4 + IPv6 (ICMPv6 Ping)
 * Trace, verbose logging, DropTail queue, named trace file.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv6-ping-helper.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnPing6Example");

void
QueueTrace (Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> pkt)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds ()
                       << " " << context
                       << " Packet UID " << pkt->GetUid ()
                       << " Size = " << pkt->GetSize ()
                       << std::endl;
}

void
ReceptionTrace (Ptr<OutputStreamWrapper> stream, std::string context,
                Ptr<const Packet> packet, const Address &address)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds ()
                       << " " << context
                       << " Packet UID " << packet->GetUid ()
                       << " Received at " << address
                       << std::endl;
}

int
main (int argc, char *argv[])
{
  // Enable logging for debug
  LogComponentEnable ("WsnPing6Example", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_WARN);
  LogComponentEnable ("Ipv6Interface", LOG_LEVEL_WARN);
  LogComponentEnable ("LrWpanMac", LOG_LEVEL_WARN);
  LogComponentEnable ("LrWpanPhy", LOG_LEVEL_WARN);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Mobility (stationary)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Create 802.15.4 devices and channel
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);
  lrWpanHelper.AssociateToPan (lrWpanDevices, 0x1234);

  // Enable Trace Sources on DropTail queues (for test/tracing)
  // DropTail on mesh-under MAC, so not explicitly here, unless we add queue tracing at the device
  
  // Install 6LoWPAN adaptation layer
  SixLowPanHelper sixlowpan;
  NetDeviceContainer lowpanDevices = sixlowpan.Install (lrWpanDevices);

  // IPv6 stack
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifaces = ipv6.Assign (lowpanDevices);
  ifaces.SetForwarding (0, true); // if needed for routing
  ifaces.SetForwarding (1, true);
  ifaces.SetDefaultRouteInAllNodes (0);

  // Trace setup
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("wsn-ping6.tr");

  // Connect queue trace sources if the devices have queues
  // For LrWpanNetDevice, it may not have a DropTail queue; but 6LoWPAN edge uses QueueDisc on output
  // For demonstration, we will connect packet receptions on device and application layer

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<NetDevice> dev = lowpanDevices.Get (i);

      // Trace packet reception at device
      dev->TraceConnectWithoutContext ("MacRx", MakeBoundCallback (&QueueTrace, stream));

      // Trace packet reception at IPv6 layer
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      ipv6->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&QueueTrace, stream));
    }

  // Application: ICMPv6 Echo (Ping6)
  // node0 pings node1

  Ipv6Address destAddr = ifaces.GetAddress (1, 1); // link-local is index 0, global is 1

  Ipv6PingHelper ping6 (destAddr);
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Size", UintegerValue (32));
  ApplicationContainer app = ping6.Install (nodes.Get (0));
  app.Start (Seconds (2.0));
  app.Stop (Seconds (12.0));

  // Also log app-level flows/echo replies
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V6Ping/Rx", MakeBoundCallback (&ReceptionTrace, stream));

  NS_LOG_INFO ("Running simulation...");
  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation done.");

  return 0;
}