#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/packet.h"
#include "ns3/packet-socket.h"
#include "ns3/queue.h"
#include "ns3/queue-size.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoLanExample");

void
QueueLengthTrace (uint32_t oldLength, uint32_t newLength)
{
  static std::ofstream outfile ("udp-echo.tr", std::ios_base::app);
  outfile << Simulator::Now ().GetSeconds () << " QueueLength " << newLength << std::endl;
}

void
PacketReceiveTrace (Ptr<const Packet> pkt, const Address & addr)
{
  static std::ofstream outfile ("udp-echo.tr", std::ios_base::app);
  outfile << Simulator::Now ().GetSeconds () << " PacketReceived " << pkt->GetSize () << " bytes" << std::endl;
}

int
main (int argc, char *argv[])
{
  std::string proto = "IPv4";

  CommandLine cmd;
  cmd.AddValue ("protocol", "IP protocol to use: IPv4 or IPv6", proto);
  cmd.Parse(argc, argv);

  bool useIpv6 = (proto == "IPv6" || proto == "ipv6");

  // Create 4 node LAN
  NodeContainer nodes;
  nodes.Create (4);

  // Create CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560))); // default for CSMA
  csma.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize ("100p")));

  NetDeviceContainer devices = csma.Install (nodes);

  // Enable tracing for DropTail queue
  Ptr<CsmaNetDevice> csmaDev0 = DynamicCast<CsmaNetDevice> (devices.Get (0));
  Ptr<Queue<Packet>> queue = csmaDev0->GetQueue ();
  // Trace queue length
  queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&QueueLengthTrace));

  // Install Internet Stack
  InternetStackHelper stack;
  if (useIpv6)
    {
      stack.SetIpv4StackInstall (false);
      stack.SetIpv6StackInstall (true);
    }
  else
    {
      stack.SetIpv4StackInstall (true);
      stack.SetIpv6StackInstall (false);
    }
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;
  Address serverAddr;

  if (useIpv6)
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:1234::", 64);
      ipv6Interfaces = ipv6.Assign (devices);
      serverAddr = Inet6SocketAddress (ipv6Interfaces.GetAddress (1, 1), 9); // n1 global address
    }
  else
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4Interfaces = ipv4.Assign (devices);
      serverAddr = InetSocketAddress (ipv4Interfaces.GetAddress (1), 9); // n1
    }

  // Install UDP Echo Server on node 1
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Echo Client on node 0, targeting node 1
  UdpEchoClientHelper echoClient (serverAddr, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Trace packet reception on server side
  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Application> app = serverApps.Get (0);
  Ptr<UdpEchoServer> udpServer = DynamicCast<UdpEchoServer> (app);

  // Trace the server's Rx socket
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback (&PacketReceiveTrace));

  // Clean previous traces
  std::ofstream clearfile ("udp-echo.tr");
  clearfile.close ();

  // Enable pcap tracing for debugging (optional)
  //csma.EnablePcapAll ("udp-echo-lan");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}