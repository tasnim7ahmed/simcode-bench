#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

void
RxEvent(Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream rxTrace;
  static bool opened = false;
  if (!opened)
    {
      rxTrace.open("fragmentation-ipv6.tr", std::ios_base::app);
      opened = true;
    }
  rxTrace << Simulator::Now().GetSeconds() << " RECEIVED " << packet->GetSize() << " bytes\n";
}

void
QueueEvent(Ptr<const QueueItem> item)
{
  static std::ofstream queueTrace;
  static bool opened = false;
  if (!opened)
    {
      queueTrace.open("fragmentation-ipv6.tr", std::ios_base::app);
      opened = true;
    }
  queueTrace << Simulator::Now().GetSeconds() << " QUEUED " << item->GetPacket()->GetSize() << " bytes\n";
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3); // n0, r, n1

  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> r = nodes.Get(1);
  Ptr<Node> n1 = nodes.Get(2);

  // CSMA channels
  NodeContainer n0r(r, n0), rn1(r, n1);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer d0r = csma.Install(n0r);
  NetDeviceContainer dr1 = csma.Install(rn1);

  // Internet stack
  InternetStackHelper stack;
  stack.SetIpv4StackInstall(false);
  stack.Install(nodes);

  // IPv6 assignment
  Ipv6AddressHelper ipv6;
  Ipv6InterfaceContainer i0r, ir1;

  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  i0r = ipv6.Assign(d0r);

  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  ir1 = ipv6.Assign(dr1);

  i0r.SetForwarding(1, true);   // router interface
  ir1.SetForwarding(0, true);   // router interface

  i0r.SetDefaultRouteInAllNodes(1); // n0 -> r
  ir1.SetDefaultRouteInAllNodes(1); // n1 -> r

  // UDP Echo server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(n1);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo client on n0; large packet size to force fragmentation
  UdpEchoClientHelper echoClient(ir1.GetAddress(0,1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(2500)); // likely exceeds MTU

  ApplicationContainer clientApps = echoClient.Install(n0);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable tracing: queue and Rx events
  Ptr<NetDevice> dev = d0r.Get(0);
  Ptr<CsmaNetDevice> csmaDev = dev->GetObject<CsmaNetDevice>();
  Ptr<Queue<Packet>> queue = csmaDev->GetQueue();
  if (queue)
    {
      queue->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueEvent));
    }

  Ptr<Node> lastHopNode = n1;
  Ptr<NetDevice> lastDev = dr1.Get(1);
  Ptr<Ipv6> ipv6Obj = lastHopNode->GetObject<Ipv6>();
  uint32_t ifIndex = ipv6Obj->GetInterfaceForDevice(lastDev);
  ipv6Obj->TraceConnectWithoutContext("Rx", MakeCallback(&RxEvent));

  // Enable pcap tracing
  csma.EnablePcapAll("fragmentation-ipv6", true);

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}