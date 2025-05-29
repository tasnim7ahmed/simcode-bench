#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoLanExample");

int
main (int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Enable IPv6 instead of IPv4", useIpv6);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpEchoLanExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  NS_LOG_INFO ("Create CSMA channel.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  if (useIpv6)
    {
      NS_LOG_INFO ("Assigning IPv6 Addresses.");
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:1::", 64);
      Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);
      interfaces.SetForwarding (0, true);
      interfaces.SetDefaultRouteInAllNodes (0);
    }
  else
    {
      NS_LOG_INFO ("Assigning IPv4 Addresses.");
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4.Assign (devices);
    }

  NS_LOG_INFO ("Install UDP Echo Server on n1.");
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  NS_LOG_INFO ("Install UDP Echo Client on n0.");
  Address serverAddress;
  if (useIpv6)
    {
      Ptr<Ipv6> ipv6 = nodes.Get (1)->GetObject<Ipv6> ();
      serverAddress = Inet6SocketAddress (ipv6->GetAddress (1, 1).GetAddress (), port);
    }
  else
    {
      Ptr<Ipv4> ipv4 = nodes.Get (1)->GetObject<Ipv4> ();
      serverAddress = InetSocketAddress (ipv4->GetAddress (1, 0).GetLocal (), port);
    }

  UdpEchoClientHelper echoClient (serverAddress, port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("Enable pcap tracing.");
  csma.EnablePcapAll ("udp-echo", true);

  NS_LOG_INFO ("Set up ASCII tracing to file.");
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("udp-echo.tr");
  csma.EnableAsciiAll (stream);

  NS_LOG_INFO ("Enable queue tracing.");
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> nd = devices.Get (i);
      Ptr<Queue<Packet> > queue = nd->GetObject<Queue<Packet> > ();
      if (queue)
        {
          queue->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (
            [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> pkt)
            {
              *stream->GetStream () << Simulator::Now ().GetSeconds ()
                                   << " Queue Enqueue " << pkt->GetSize () << " bytes" << std::endl;
            }, stream));
          queue->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (
            [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> pkt)
            {
              *stream->GetStream () << Simulator::Now ().GetSeconds ()
                                   << " Queue Dequeue " << pkt->GetSize () << " bytes" << std::endl;
            }, stream));
          queue->TraceConnectWithoutContext ("Drop", MakeBoundCallback (
            [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> pkt)
            {
              *stream->GetStream () << Simulator::Now ().GetSeconds ()
                                   << " Queue Drop " << pkt->GetSize () << " bytes" << std::endl;
            }, stream));
        }
    }

  NS_LOG_INFO ("Enable packet reception tracing.");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          Ptr<NetDevice> dev = node->GetDevice (j);
          dev->TraceConnectWithoutContext ("MacRx", MakeBoundCallback (
            [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> pkt)
            {
              *stream->GetStream () << Simulator::Now ().GetSeconds ()
                                   << " Packet Received " << pkt->GetSize () << " bytes" << std::endl;
            }, stream));
        }
    }

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation Complete.");
  return 0;
}