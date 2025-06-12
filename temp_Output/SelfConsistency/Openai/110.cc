#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/ping6-helper.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleIpv6PingCsmaExample");

void
PacketReceiveCallback (Ptr<const Packet> packet, const Address & addr)
{
  std::ofstream outFile;
  outFile.open ("ping6.tr", std::ios_base::app);
  outFile << Simulator::Now ().GetSeconds () << " RECEIVED packet of " << packet->GetSize () << " bytes\n";
  outFile.close ();
}

void
TraceQueue (Ptr<Queue<Packet>> queue, std::string context)
{
  std::ofstream outFile;
  outFile.open ("ping6.tr", std::ios_base::app);
  outFile << Simulator::Now ().GetSeconds () << " QUEUE " << context << " packets in queue: " << queue->GetNPackets () << "\n";
  outFile.close ();
}

int
main (int argc, char *argv[])
{
  // Simulation parameters
  uint32_t nPackets = 5;
  double interval = 1.0;

  // Enable logging
  // LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);

  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // 2. Create LAN: csma channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (100));

  NetDeviceContainer devices = csma.Install (nodes);

  // 3. Install Internet stack (IPv6)
  InternetStackHelper stack;
  stack.Install (nodes);

  // 4. Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  // Set interfaces up and assign link-local (if needed)
  interfaces.SetForwarding (0, true);
  interfaces.SetForwarding (1, true);

  interfaces.SetDefaultRouteInAllNodes (0);

  // 5. Install Ping6 application on n0, ping n1
  Ping6Helper ping6;
  ping6.SetLocal (interfaces.GetAddress (0, 1)); // Global address of n0
  ping6.SetRemote (interfaces.GetAddress (1, 1)); // Global address of n1
  ping6.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  ping6.SetAttribute ("PacketSize", UintegerValue (56));

  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (1.0 + nPackets * interval + 1.0));

  // 6. Trace queues on both interfaces
  Ptr<Queue<Packet>> queue0 = devices.Get (0)->GetObject<NetDevice> ()->GetObject<PointToPointNetDevice> () ?
    devices.Get (0)->GetObject<PointToPointNetDevice> ()->GetQueue () :
    devices.Get (0)->GetObject<CsmaNetDevice> ()->GetQueue ();

  Ptr<Queue<Packet>> queue1 = devices.Get (1)->GetObject<NetDevice> ()->GetObject<PointToPointNetDevice> () ?
    devices.Get (1)->GetObject<PointToPointNetDevice> ()->GetQueue () :
    devices.Get (1)->GetObject<CsmaNetDevice> ()->GetQueue ();

  // Use Config::ConnectWithoutContext if you only have Ptr, but we want to distinguish them:
  Simulator::Schedule (Seconds (1.0), &TraceQueue, queue0, "n0");
  Simulator::Schedule (Seconds (1.0), &TraceQueue, queue1, "n1");

  // Optionally, sample later as well, but for brevity, only once at start.

  // 7. Trace ICMPv6 echo reply packet receptions on n0 and n1 (via their interfaces)
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devices.Get (i);
      dev->TraceConnectWithoutContext ("MacRx", MakeCallback (&PacketReceiveCallback));
    }

  // 8. Run simulation
  Simulator::Stop (Seconds (1.0 + nPackets * interval + 2.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}