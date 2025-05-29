#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/icmpv6-echo.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleIpv6PingExample");

static std::ofstream g_traceFile;

void
QueueEnqueueTrace(uint32_t nodeId, std::string context, Ptr<const Packet> packet)
{
  g_traceFile << Simulator::Now().GetSeconds() << " " << nodeId << " Enqueue "
              << packet->GetSize() << std::endl;
}

void
QueueDequeueTrace(uint32_t nodeId, std::string context, Ptr<const Packet> packet)
{
  g_traceFile << Simulator::Now().GetSeconds() << " " << nodeId << " Dequeue "
              << packet->GetSize() << std::endl;
}

void
QueueDropTrace(uint32_t nodeId, std::string context, Ptr<const Packet> packet)
{
  g_traceFile << Simulator::Now().GetSeconds() << " " << nodeId << " Drop "
              << packet->GetSize() << std::endl;
}

void
PacketReceptionTrace(std::string context, Ptr<const Packet> p, const Address &address)
{
  g_traceFile << Simulator::Now().GetSeconds() << " Rx "
              << p->GetSize() << std::endl;
}

int main(int argc, char *argv[])
{
  uint32_t nPackets = 5;
  double simTime = 5.0;

  g_traceFile.open("ping6.tr", std::ios::out);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetQueue("ns3::DropTailQueue<Packet>");

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

  // Activate interfaces
  interfaces.SetForwarding(0, true);
  interfaces.SetForwarding(1, true);
  interfaces.SetDefaultRouteInAllNodes(0);

  // Tracing queues
  for (uint32_t i = 0; i < devices.GetN(); ++i)
  {
    Ptr<NetDevice> dev = devices.Get(i);
    Ptr<Queue<Packet>> queue = dev->GetObject<CsmaNetDevice>()->GetQueue();

    if (queue)
    {
      std::ostringstream oss;
      oss << "Node" << i;
      queue->TraceConnect<std::string, Ptr<const Packet>>(
        "Enqueue", oss.str(), MakeBoundCallback(&QueueEnqueueTrace, i));
      queue->TraceConnect<std::string, Ptr<const Packet>>(
        "Dequeue", oss.str(), MakeBoundCallback(&QueueDequeueTrace, i));
      queue->TraceConnect<std::string, Ptr<const Packet>>(
        "Drop", oss.str(), MakeBoundCallback(&QueueDropTrace, i));
    }
  }

  // Tracing receptions at node 1
  devices.Get(1)->TraceConnect("PhyRxDrop", "", MakeCallback(&PacketReceptionTrace));
  nodes.Get(1)->GetObject<Ipv6>()->TraceConnect("Rx", "", MakeCallback(&PacketReceptionTrace));

  // Set up ICMPv6 echo application (Ping6)
  uint16_t echoPort = 0; // Not used for ICMPv6
  Ipv6Address dstAddr = interfaces.GetAddress(1, 1); // Link-local at index 0, global at 1

  V6PingHelper ping(dstAddr);
  ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ping.SetAttribute("Size", UintegerValue(56));
  ping.SetAttribute("Count", UintegerValue(nPackets));

  ApplicationContainer apps = ping.Install(nodes.Get(0));
  apps.Start(Seconds(0.1));
  apps.Stop(Seconds(simTime));

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  g_traceFile.close();

  return 0;
}