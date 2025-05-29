#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

void
QueueLengthTrace(std::string context, uint32_t queueLength)
{
  static std::ofstream outFile("udp-echo.tr", std::ios::app);
  outFile << Simulator::Now().GetSeconds() << " " << context << " QueueLength " << queueLength << std::endl;
}

void
RxTrace(std::string context, Ptr<const Packet> p)
{
  static std::ofstream outFile("udp-echo.tr", std::ios::app);
  outFile << Simulator::Now().GetSeconds() << " " << context << " PacketReceived " << p->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  std::string netType = "ipv4";
  CommandLine cmd;
  cmd.AddValue("type", "IP type: ipv4 or ipv6", netType);
  cmd.Parse(argc, argv);

  bool useIpv6 = (netType == "ipv6");

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  csma.SetQueue ("ns3::DropTailQueue<Packet>");

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer ipv4Ifs;
  Ipv6InterfaceContainer ipv6Ifs;
  if (useIpv6)
    {
      ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
      ipv6Ifs = ipv6.Assign(devices);
      ipv6Ifs.SetForwarding(0, true); ipv6Ifs.SetDefaultRouteInAllNodes(0);
    }
  else
    {
      ipv4.SetBase("10.1.1.0", "255.255.255.0");
      ipv4Ifs = ipv4.Assign(devices);
    }

  uint16_t echoPort = 9;
  Address serverAddress;
  if (useIpv6)
    {
      serverAddress = Inet6SocketAddress(ipv6Ifs.GetAddress(1,1), echoPort);
    }
  else
    {
      serverAddress = InetSocketAddress(ipv4Ifs.GetAddress(1), echoPort);
    }

  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(serverAddress, echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(9.0));

  // Enable queue length tracing
  Ptr<CsmaNetDevice> csmaDev = devices.Get(0)->GetObject<CsmaNetDevice>();
  Ptr<Queue<Packet> > queue = csmaDev->GetQueue();
  if (!queue)
    {
      queue = csmaDev->GetObject<Queue<Packet> >();
    }
  if (queue)
    {
      queue->TraceConnect("PacketsInQueue", "queue", MakeCallback(&QueueLengthTrace));
    }

  // Enable packet reception tracing
  for (uint32_t i=0; i<devices.GetN(); ++i)
    {
      devices.Get(i)->TraceConnect("MacRx", "device"+std::to_string(i), MakeCallback(&RxTrace));
    }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}