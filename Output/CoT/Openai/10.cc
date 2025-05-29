#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoLan");

static void
PacketReceiveCallback (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("Packet received of size " << packet->GetSize ());
}

int
main (int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Enable IPv6 if true, IPv4 if false", useIpv6);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpEchoLan", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Creating nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  NS_LOG_INFO ("Setting up CSMA channel.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));
  NetDeviceContainer devices = csma.Install (nodes);

  NS_LOG_INFO ("Installing internet stack.");
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6AddressHelper ipv6;
  ipv6.SetBase ("2001:db8:1::", 64);
  Ipv6InterfaceContainer ipv6Interfaces;

  if (useIpv6)
    {
      ipv6Interfaces = ipv6.Assign (devices);
      for (uint32_t i = 0; i < ipv6Interfaces.GetN (); ++i)
        {
          ipv6Interfaces.SetForwarding (i, true);
          ipv6Interfaces.SetDefaultRouteInAllNodes (i);
        }
    }
  else
    {
      ipv4Interfaces = ipv4.Assign (devices);
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  NS_LOG_INFO ("Installing UDP echo server on node 1.");
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  NS_LOG_INFO ("Installing UDP echo client on node 0.");
  Address serverAddress;
  if (useIpv6)
    {
      serverAddress = Inet6SocketAddress (ipv6Interfaces.GetAddress (1, 1), echoPort);
    }
  else
    {
      serverAddress = InetSocketAddress (ipv4Interfaces.GetAddress (1), echoPort);
    }
  UdpEchoClientHelper echoClient (serverAddress);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("Enabling PCAP tracing.");
  csma.EnablePcapAll ("udp-echo", true);

  NS_LOG_INFO ("Enabling ASCII tracing and queue event tracing.");
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("udp-echo.tr");
  csma.EnableAsciiAll (stream);

  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<CsmaNetDevice> csmaDev = dev->GetObject<CsmaNetDevice> ();
      if (csmaDev != nullptr)
        {
          Ptr<Queue<Packet> > queue = csmaDev->GetQueue ();
          if (queue)
            {
              queue->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&QueueTrace, stream, "Enqueue"));
              queue->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&QueueTrace, stream, "Dequeue"));
              queue->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&QueueTrace, stream, "Drop"));
            }
        }
    }

  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Ipv4> ipv4Obj = n1->GetObject<Ipv4> ();
  Ptr<Ipv6> ipv6Obj = n1->GetObject<Ipv6> ();
  Ptr<NetDevice> dev = devices.Get (1);

  if (useIpv6)
    {
      dev->GetObject<NetDevice> ()->TraceConnectWithoutContext ("MacRx", MakeCallback (&PacketReceiveCallback));
    }
  else
    {
      dev->GetObject<NetDevice> ()->TraceConnectWithoutContext ("MacRx", MakeCallback (&PacketReceiveCallback));
    }

  NS_LOG_INFO ("Running simulation.");
  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation complete.");
  return 0;
}

static void
QueueTrace (Ptr<OutputStreamWrapper> stream, std::string event, Ptr<const Packet> packet)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << event << " " << packet->GetSize () << std::endl;
}