#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoLanTrace");

void
QueueLengthTrace(std::string context, uint32_t oldValue, uint32_t newValue)
{
  static std::ofstream outFile("udp-echo.tr", std::ios::app);
  outFile << Simulator::Now ().GetSeconds () << " " << context
          << " QueueLength " << newValue << std::endl;
}

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream outFile("udp-echo.tr", std::ios::app);
  outFile << Simulator::Now ().GetSeconds () << " " << context
          << " PacketReceived " << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Enable IPv6 (default = false, uses IPv4)", useIpv6);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma.SetQueue ("ns3::DropTailQueue<Packet>");

  NetDeviceContainer devices = csma.Install (nodes);

  // Queue tracing
  Ptr<Queue<Packet> > queue = DynamicCast<PointToPointNetDevice> (devices.Get(0)) == 0 ?
    devices.Get (0)->GetObject<CsmaNetDevice> ()->GetQueue () :
    devices.Get (0)->GetObject<PointToPointNetDevice> ()->GetQueue ();
  queue->TraceConnect ("PacketsInQueue", "csma/queue", MakeCallback (&QueueLengthTrace));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;
  std::string serverIp;
  if (!useIpv6)
    {
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4Interfaces = ipv4.Assign (devices);
      serverIp = ipv4Interfaces.GetAddress (1).ToString ();
    }
  else
    {
      ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
      ipv6Interfaces = ipv6.Assign (devices);
      serverIp = ipv6Interfaces.GetAddress (1,1).GetAddress ().ToString ();
    }

  uint16_t echoPort = 9;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (!useIpv6)
    {
      UdpEchoServerHelper echoServer (echoPort);
      serverApps = echoServer.Install (nodes.Get (1));
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (10.0));

      UdpEchoClientHelper echoClient (ipv4Interfaces.GetAddress (1), echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      clientApps = echoClient.Install (nodes.Get (0));
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (10.0));

      // Packet reception tracing
      Ptr<Application> app = serverApps.Get (0);
      Ptr<Socket> sock = DynamicCast<UdpEchoServer> (app)->GetSocket ();
      sock->TraceConnectWithoutContext ("Recv", MakeBoundCallback (&RxTrace, "server"));
    }
  else
    {
      UdpEchoServerHelper echoServer (echoPort);
      serverApps = echoServer.Install (nodes.Get (1));
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (10.0));

      UdpEchoClientHelper echoClient (ipv6Interfaces.GetAddress (1, 1), echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      clientApps = echoClient.Install (nodes.Get (0));
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (10.0));

      // Packet reception tracing
      Ptr<Application> app = serverApps.Get (0);
      Ptr<Socket> sock = DynamicCast<UdpEchoServer> (app)->GetSocket ();
      sock->TraceConnectWithoutContext ("Recv", MakeBoundCallback (&RxTrace, "server"));
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}