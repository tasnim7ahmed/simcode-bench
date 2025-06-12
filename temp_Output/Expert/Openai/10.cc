#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoCsmaLan");

void
Ipv4RxTrace (std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
  NS_LOG_INFO ("[IPv4 RX] Context: " << context << " Packet: " << packet->GetSize () << " bytes Interface: " << interface);
}

void
Ipv6RxTrace (std::string context, Ptr<const Packet> packet, Ptr<Ipv6> ipv6, uint32_t interface)
{
  NS_LOG_INFO ("[IPv6 RX] Context: " << context << " Packet: " << packet->GetSize () << " bytes Interface: " << interface);
}

int
main (int argc, char *argv[])
{
  bool enableIpv6 = false;
  std::string trFile = "udp-echo.tr";

  CommandLine cmd;
  cmd.AddValue ("enableIpv6", "Enable IPv6 instead of IPv4", enableIpv6);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpEchoCsmaLan", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Creating nodes...");
  NodeContainer nodes;
  nodes.Create (4);

  NS_LOG_INFO ("Setting up CSMA channel...");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer devices = csma.Install (nodes);

  NS_LOG_INFO ("Installing Internet stack...");
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;
  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;

  if (!enableIpv6)
    {
      NS_LOG_INFO ("Assigning IPv4 addresses...");
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4Interfaces = ipv4.Assign (devices);
    }
  else
    {
      NS_LOG_INFO ("Assigning IPv6 addresses...");
      ipv6.SetBase ("2001:1::", 64);
      ipv6Interfaces = ipv6.Assign (devices);
      ipv6.SetRouter (nodes, 0);
    }

  NS_LOG_INFO ("Creating applications...");

  uint16_t port = 9;
  double startTime = 2.0;
  double stopTime = 10.0;

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (stopTime + 1));

  Address destAddress;
  if (!enableIpv6)
    {
      destAddress = InetSocketAddress (ipv4Interfaces.GetAddress (1), port);
    }
  else
    {
      destAddress = Inet6SocketAddress (ipv6Interfaces.GetAddress (1,1), port);
    }

  UdpEchoClientHelper echoClient (destAddress);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (startTime));
  clientApps.Stop (Seconds (stopTime));

  NS_LOG_INFO ("Enabling tracing...");

  csma.EnablePcapAll ("udp-echo", true);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (trFile);

  csma.EnableAsciiAll (stream);

  QueueDiscContainer qdiscs;
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<CsmaNetDevice> csmaDev = dev->GetObject<CsmaNetDevice> ();
      if (csmaDev)
        {
          Ptr<Queue<Packet> > queue = csmaDev->GetQueue ();
          if (queue)
            {
              queue->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
                  *stream->GetStream () << Simulator::Now ().GetSeconds () << " Enqueue " << p->GetSize () << " bytes" << std::endl;
                }, stream));
              queue->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
                  *stream->GetStream () << Simulator::Now ().GetSeconds () << " Dequeue " << p->GetSize () << " bytes" << std::endl;
                }, stream));
              queue->TraceConnectWithoutContext ("Drop", MakeBoundCallback (
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
                  *stream->GetStream () << Simulator::Now ().GetSeconds () << " Drop " << p->GetSize () << " bytes" << std::endl;
                }, stream));
            }
        }
    }

  if (!enableIpv6)
    {
      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<Ipv4> ipv4Node = nodes.Get (i)->GetObject<Ipv4> ();
          ipv4Node->TraceConnect ("Rx", "", MakeCallback (&Ipv4RxTrace));
        }
    }
  else
    {
      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<Ipv6> ipv6Node = nodes.Get (i)->GetObject<Ipv6> ();
          ipv6Node->TraceConnect ("Rx", "", MakeCallback (&Ipv6RxTrace));
        }
    }

  NS_LOG_INFO ("Running simulation...");
  Simulator::Stop (Seconds (stopTime + 2));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}