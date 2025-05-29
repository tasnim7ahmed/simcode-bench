#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/radvd-helper.h"
#include <fstream>

using namespace ns3;

void PacketRxTrace(Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream rxTrace("radvd.tr", std::ios_base::app);
  rxTrace << Simulator::Now().GetSeconds() << " Packet received, size=" << packet->GetSize() << std::endl;
}

void EnqueueTrace(Ptr<const Packet> packet)
{
  static std::ofstream rxTrace("radvd.tr", std::ios_base::app);
  rxTrace << Simulator::Now().GetSeconds() << " Packet enqueued, size=" << packet->GetSize() << std::endl;
}

void DequeueTrace(Ptr<const Packet> packet)
{
  static std::ofstream rxTrace("radvd.tr", std::ios_base::app);
  rxTrace << Simulator::Now().GetSeconds() << " Packet dequeued, size=" << packet->GetSize() << std::endl;
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2); // n0, n1

  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);

  Ptr<Node> router = CreateObject<Node>();

  NodeContainer net1(n0, router);
  NodeContainer net2(router, n1);

  CsmaHelper csma1;
  csma1.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma1.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  CsmaHelper csma2;
  csma2.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma2.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devNet1 = csma1.Install(net1);
  NetDeviceContainer devNet2 = csma2.Install(net2);

  InternetStackHelper internetv6;
  internetv6.Install(n0);
  internetv6.Install(n1);
  internetv6.Install(router);

  Ipv6AddressHelper ipv6_1;
  ipv6_1.SetBase("2001:1::", Ipv6Prefix(64));
  Ipv6InterfaceContainer ifaceNet1 = ipv6_1.Assign(devNet1);
  ifaceNet1.SetForwarding(1, true);
  ifaceNet1.SetDefaultRouteInAllNodes(1);

  Ipv6AddressHelper ipv6_2;
  ipv6_2.SetBase("2001:2::", Ipv6Prefix(64));
  Ipv6InterfaceContainer ifaceNet2 = ipv6_2.Assign(devNet2);
  ifaceNet2.SetForwarding(0, true);
  ifaceNet2.SetDefaultRouteInAllNodes(0);

  // Setup Router Advertisements using Radvd
  RadvdHelper radvdHelper;

  RadvdInterface radvIface1;
  radvIface1.SetInterface(ifaceNet1.GetInterfaceIndex(1));
  radvIface1.SetPrefix("2001:1::", 64);

  RadvdInterface radvIface2;
  radvIface2.SetInterface(ifaceNet2.GetInterfaceIndex(0));
  radvIface2.SetPrefix("2001:2::", 64);

  std::vector<RadvdInterface> ifaces;
  ifaces.push_back(radvIface1);
  ifaces.push_back(radvIface2);

  ApplicationContainer radvd = radvdHelper.Install(router, ifaces);
  radvd.Start(Seconds(1.0));
  radvd.Stop(Seconds(20.0));

  // ICMPv6 Ping6 Application from n0 to n1
  uint32_t packetSize = 56;
  uint8_t maxPackets = 5;
  Time interPacketInterval = Seconds(1.0);

  V6PingHelper ping6(ifaceNet2.GetAddress(1, 1)); // Ping n1
  ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
  ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer pinger = ping6.Install(n0);
  pinger.Start(Seconds(2.0));
  pinger.Stop(Seconds(19.0));

  // Tracing
  devNet1.Get(0)->GetObject<Queue<Packet>>()->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueTrace));
  devNet1.Get(0)->GetObject<Queue<Packet>>()->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueTrace));
  devNet2.Get(1)->GetObject<Queue<Packet>>()->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueTrace));
  devNet2.Get(1)->GetObject<Queue<Packet>>()->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueTrace));

  devNet2.Get(1)->TraceConnectWithoutContext("MacRx", MakeCallback(&PacketRxTrace));
  devNet1.Get(0)->TraceConnectWithoutContext("MacRx", MakeCallback(&PacketRxTrace));

  Simulator::Stop(Seconds(21.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}