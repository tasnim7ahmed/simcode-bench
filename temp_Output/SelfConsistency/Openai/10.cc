/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ns-3 simulation: UDP Echo Server/Client on CSMA LAN with IPv4/IPv6 option.
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaUdpEchoExample");

int
main (int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Use IPv6 (default is false/IPv4)", useIpv6);
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("CsmaUdpEchoExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  // Create CSMA channel
  CSMAHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  // Install CSMA devices to nodes
  NS_LOG_INFO ("Install CSMA devices.");
  NetDeviceContainer devices = csma.Install (nodes);

  // Install Internet stack
  NS_LOG_INFO ("Install Internet stack.");
  InternetStackHelper internet;
  if (useIpv6)
    {
      internet.Install (nodes);
    }
  else
    {
      internet.Install (nodes);
    }

  // Assign IP addresses
  NS_LOG_INFO ("Assign IP addresses.");
  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer ipv4Ifs;
  Ipv6InterfaceContainer ipv6Ifs;
  if (useIpv6)
    {
      ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
      ipv6Ifs = ipv6.Assign (devices);
      // No need for link-local routing in this flat LAN case
    }
  else
    {
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4Ifs = ipv4.Assign (devices);
    }

  // Application settings
  uint16_t echoPort = 9;
  uint32_t maxPacketCount = 1;
  Time interPacketInterval = Seconds (1.0);
  uint32_t packetSize = 1024;

  // UDP Echo Server on n1
  NS_LOG_INFO ("Install UDP Echo Server on n1.");
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.5));

  // UDP Echo Client on n0
  NS_LOG_INFO ("Install UDP Echo Client on n0.");
  Address dstAddress;
  if (useIpv6)
    {
      dstAddress = Inet6SocketAddress (ipv6Ifs.GetAddress (1, 1), echoPort);
    }
  else
    {
      dstAddress = InetSocketAddress (ipv4Ifs.GetAddress (1), echoPort);
    }
  UdpEchoClientHelper echoClient (dstAddress);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing: ASCII event tracing
  CSMAHelper csmaTrace;
  std::ofstream ascii;
  ascii.open ("udp-echo.tr");
  csma.EnableAsciiAll (ascii);
  csma.EnablePcapAll ("udp-echo", true);

  // Enable queue tracing
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice> (dev);
      if (csmaDev)
        {
          Ptr<Queue<Packet>> queue = csmaDev->GetQueue ();
          if (queue)
            {
              queue->TraceConnectWithoutContext ("Enqueue", MakeCallback (
                [](Ptr<const Packet> p) {
                  NS_LOG_INFO ("Packet enqueued at time " << Simulator::Now ().GetSeconds () << "s, size=" << p->GetSize ());
                }));
              queue->TraceConnectWithoutContext ("Dequeue", MakeCallback (
                [](Ptr<const Packet> p) {
                  NS_LOG_INFO ("Packet dequeued at time " << Simulator::Now ().GetSeconds () << "s, size=" << p->GetSize ());
                }));
              queue->TraceConnectWithoutContext ("Drop", MakeCallback (
                [](Ptr<const Packet> p) {
                  NS_LOG_INFO ("Packet dropped at time " << Simulator::Now ().GetSeconds () << "s, size=" << p->GetSize ());
                }));
            }
        }
    }

  // Trace packet receptions at n1
  devices.Get (1)->TraceConnectWithoutContext (
    "MacRx",
    MakeCallback ([](Ptr<const Packet> p) {
      NS_LOG_INFO ("Packet received at node n1 MAC at " << Simulator::Now ().GetSeconds () << "s, size=" << p->GetSize ());
    })
  );
  devices.Get (0)->TraceConnectWithoutContext (
    "MacRx",
    MakeCallback ([](Ptr<const Packet> p) {
      NS_LOG_INFO ("Packet received at node n0 MAC at " << Simulator::Now ().GetSeconds () << "s, size=" << p->GetSize ());
    })
  );

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  ascii.close ();
  NS_LOG_INFO ("Done.");
  return 0;
}