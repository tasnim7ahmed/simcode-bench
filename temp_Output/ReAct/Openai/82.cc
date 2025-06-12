#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoLanSimulation");

std::ofstream traceFile;

// Trace function for queue length
void
QueueLengthTrace(uint32_t oldValue, uint32_t newValue)
{
  Simulator::ScheduleNow(&QueueLengthTrace, newValue, newValue);
}

void
QueueLengthTraceCallback(uint32_t size)
{
  traceFile << Simulator::Now ().GetSeconds () << " QUEUE_LENGTH " << size << std::endl;
}

// Packet reception callback
void
RxPacketTrace(Ptr<const Packet> p, const Address &from)
{
  traceFile << Simulator::Now ().GetSeconds () << " RX_PACKET " << p->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue("useIpv6", "Set to true to use IPv6", useIpv6);
  cmd.Parse (argc, argv);

  traceFile.open("udp-echo.tr", std::ios_base::out);

  NodeContainer nodes;
  nodes.Create (4);

  // Set up the LAN over CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Configure DropTail queue
  csma.SetQueue ("ns3::DropTailQueue",
     "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;

  if (!useIpv6)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4Interfaces = ipv4.Assign (devices);
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
      ipv6Interfaces = ipv6.Assign(devices);
      // autoconfig does not set global scopes up, so...
      for (uint32_t i=0; i < ipv6Interfaces.GetN (); ++i)
        {
          ipv6Interfaces.SetForwarding (i, true);
          ipv6Interfaces.SetDefaultRouteInAllNodes (i);
        }
    }

  // UDP Echo server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);

  ApplicationContainer serverApps;
  if (!useIpv6)
    {
      serverApps = echoServer.Install (nodes.Get (1));
    }
  else
    {
      // Echo server with IPv6 support is available in ns-3 >=3.36
      echoServer.SetAttribute ("Protocol", StringValue ("ns3::UdpSocketFactory"));
      serverApps = echoServer.Install (nodes.Get (1));
    }
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (useIpv6 ? Ipv6Address("2001:db8::2") : Ipv4Address("10.1.1.2"), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  if (!useIpv6)
    {
      echoClient.SetAttribute ("RemoteAddress", AddressValue(ipv4Interfaces.GetAddress (1)));
      clientApps = echoClient.Install (nodes.Get (0));
    }
  else
    {
      echoClient.SetAttribute ("RemoteIpv6Address", Ipv6AddressValue(ipv6Interfaces.GetAddress (1, 1)));
      clientApps = echoClient.Install (nodes.Get (0));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable DropTail queue tracing on one device's NetDevice queue
  Ptr<NetDevice> nd = devices.Get (0);
  Ptr<CsmaNetDevice> csmaNd = DynamicCast<CsmaNetDevice> (nd);
  Ptr<Queue<Packet>> queue = csmaNd->GetQueue ();
  if (queue)
    {
      queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&QueueLengthTraceCallback));
    }

  // Enable packet reception tracing at node n1 (server node)
  Ptr<Node> node1 = nodes.Get(1);
  Ptr<NetDevice> device1 = devices.Get(1);
  device1->TraceConnectWithoutContext("MacRx", MakeCallback(&RxPacketTrace));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  traceFile.close();

  return 0;
}