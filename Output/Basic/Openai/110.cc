#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ping6-helper.h"
#include "ns3/queue.h"
#include <fstream>

using namespace ns3;

static std::ofstream g_trFile;

void
QueueTrace(std::string context, uint32_t oldSize, uint32_t newSize)
{
  g_trFile << Simulator::Now().GetSeconds()
           << " [" << context << "] QueueSize: " << oldSize << " -> " << newSize << std::endl;
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  g_trFile << Simulator::Now().GetSeconds()
           << " [PacketRx] Packet Size: " << packet->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  uint32_t nPackets = 5;
  g_trFile.open("ping6.tr");

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma.SetQueue("ns3::DropTailQueue<Packet>");

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
  interfaces.SetForwarding(0, true);
  interfaces.SetForwarding(1, true);
  interfaces.SetDefaultRouteInAllNodes(0);

  // Connect queue size trace
  Ptr<NetDevice> dev0 = devices.Get(0);
  Ptr<CsmaNetDevice> csmaDev0 = DynamicCast<CsmaNetDevice>(dev0);
  Ptr<Queue<Packet>> queue0 = csmaDev0->GetQueue();
  queue0->TraceConnect("PacketsInQueue", "n0", MakeCallback(&QueueTrace));

  Ptr<NetDevice> dev1 = devices.Get(1);
  Ptr<CsmaNetDevice> csmaDev1 = DynamicCast<CsmaNetDevice>(dev1);
  Ptr<Queue<Packet>> queue1 = csmaDev1->GetQueue();
  queue1->TraceConnect("PacketsInQueue", "n1", MakeCallback(&QueueTrace));

  // Trace packet reception at node 1
  csmaDev1->TraceConnectWithoutContext("MacRx", MakeCallback(&RxTrace));

  // Set up Ping6 application on node 0 targeting node 1
  Ping6Helper ping6;
  ping6.SetLocal(interfaces.GetAddress(0, 1));
  ping6.SetRemote(interfaces.GetAddress(1, 1));
  ping6.SetAttribute("MaxPackets", UintegerValue(nPackets));
  ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ping6.SetAttribute("PacketSize", UintegerValue(56)); // bytes

  ApplicationContainer apps = ping6.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  g_trFile.close();
  Simulator::Destroy();
  return 0;
}