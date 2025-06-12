#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include <fstream>

using namespace ns3;

void
QueueLengthTrace(std::ofstream* stream, uint32_t oldValue, uint32_t newValue)
{
  *stream << Simulator::Now().GetSeconds() << " Queue Length: " << newValue << std::endl;
}

void
RxTrace(std::ofstream* stream, Ptr<const Packet> packet, const Address& from)
{
  *stream << Simulator::Now().GetSeconds() << " Packet Received, Size: " << packet->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd;
  cmd.AddValue("ipv6", "Use IPv6 if true, otherwise use IPv4", useIpv6);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  // Configure a DropTail queue
  csma.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (100));

  NetDeviceContainer devices = csma.Install(nodes);

  // Trace file
  std::ofstream traceStream("udp-echo.tr", std::ios::out);

  // Attach queue length tracer
  Ptr<Queue<Packet>> queue = DynamicCast<Queue<Packet>>(devices.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue());
  if (!queue)
  {
    Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(devices.Get(0));
    if (csmaDev)
    {
      Ptr<Queue<Packet>> csmaQueue = csmaDev->GetQueue();
      if (csmaQueue)
      {
        csmaQueue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&QueueLengthTrace, &traceStream));
      }
    }
  }
  else
  {
    queue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&QueueLengthTrace, &traceStream));
  }

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer i4interfaces;
  Ipv6InterfaceContainer i6interfaces;
  uint16_t echoPort = 9;

  if (useIpv6)
  {
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    i6interfaces = ipv6.Assign(devices);
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();
  }
  else
  {
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    i4interfaces = ipv4.Assign(devices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  // UDP Echo server on n1
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo client on n0
  Address serverAddress;
  if (useIpv6)
  {
    serverAddress = Inet6SocketAddress(i6interfaces.GetAddress(1,1), echoPort);
  }
  else
  {
    serverAddress = InetSocketAddress(i4interfaces.GetAddress(1), echoPort);
  }

  UdpEchoClientHelper echoClient(serverAddress, echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Trace packet receptions on the server
  Ptr<Node> serverNode = nodes.Get(1);
  Ptr<Application> serverApp = serverApps.Get(0);
  Ptr<UdpEchoServer> echoServerPtr = DynamicCast<UdpEchoServer>(serverApp);
  if (echoServerPtr)
  {
    echoServerPtr->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxTrace, &traceStream));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  traceStream.close();
  Simulator::Destroy();
  return 0;
}