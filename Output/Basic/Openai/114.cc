#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ipv6-interface.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnPing6Example");

void
PacketReceiveCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Packet received of size " << packet->GetSize());
}

void
QueueTraceCallback(std::string context, Ptr<const QueueDiscItem> item)
{
  NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Packet enqueued");
}

int main(int argc, char **argv)
{
  LogComponentEnable("WsnPing6Example", LOG_LEVEL_INFO);
  LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_WARN);
  LogComponentEnable("Ipv6EndPointDemux", LOG_LEVEL_WARN);
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable("DropTailQueue", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Install 802.15.4 devices
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);
  lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

  // Create DropTail MAC queue
  for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i)
    {
      Ptr<NetDevice> dev = lrwpanDevices.Get(i);
      Ptr<LrWpanNetDevice> wpanDev = DynamicCast<LrWpanNetDevice>(dev);
      wpanDev->GetMac()->SetAttribute("TxQueue", StringValue("ns3::DropTailQueue<Packet>"));
    }

  // Trace the queue
  Ptr<Queue<Packet>> txQueue0 = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(0))->GetMac()->GetTxQueue();
  Ptr<Queue<Packet>> txQueue1 = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(1))->GetMac()->GetTxQueue();
  txQueue0->TraceConnect("Enqueue", "queue0", MakeCallback(&QueueTraceCallback));
  txQueue1->TraceConnect("Enqueue", "queue1", MakeCallback(&QueueTraceCallback));

  // Install Internet stack with IPv6 (no IPv4)
  InternetStackHelper internetv6;
  Ipv6ListRoutingHelper listRouting;
  Ipv6StaticRoutingHelper staticRouting;
  listRouting.Add(staticRouting, 0);
  internetv6.SetIpv4StackInstall(false);
  internetv6.SetRoutingHelper(listRouting);
  internetv6.Install(nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase("2001:db8::", 64);
  Ipv6InterfaceContainer interfaces = ipv6.Assign(lrwpanDevices);
  interfaces.SetForwarding(0, true);
  interfaces.SetForwarding(1, true);

  // Set all interfaces up
  interfaces.SetDefaultRouteInAllNodes(0);

  // Install ping6 app on node 0 to ping node 1
  uint32_t packetSize = 64;
  uint32_t maxPackets = 4;
  Time interPacketInterval = Seconds(1.0);

  Ping6Helper ping6;
  ping6.SetLocal(interfaces.GetAddress(0, 1));
  ping6.SetRemote(interfaces.GetAddress(1, 1));
  ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
  ping6.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer apps = ping6.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  // Trace packet receptions
  Config::Connect("/NodeList/1/DeviceList/0/$ns3::LrWpanNetDevice/Mac/MacRx", MakeCallback(&PacketReceiveCallback));

  // Enable PCAP and ASCII tracing
  lrWpanHelper.EnablePcapAll(std::string("wsn-ping6"));
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("wsn-ping6.tr");
  lrWpanHelper.EnableAsciiAll(stream);

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}