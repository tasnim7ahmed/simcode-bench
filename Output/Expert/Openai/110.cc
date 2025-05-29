#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/applications-module.h"
#include "ns3/drop-tail-queue.h"
#include <fstream>

using namespace ns3;

static void
QueueTrace(Ptr<const QueueDiscItem> item)
{
  std::ofstream file("ping6.tr", std::ios::app);
  if (file.is_open())
    {
      file << Simulator::Now().GetSeconds() << " QUEUE "
           << item->GetPacket()->GetUid() << "\n";
      file.close();
    }
}

static void
PacketReceiveTrace(Ptr<const Packet> packet, const Address &address)
{
  std::ofstream file("ping6.tr", std::ios::app);
  if (file.is_open())
    {
      file << Simulator::Now().GetSeconds() << " RX "
           << packet->GetUid() << "\n";
      file.close();
    }
}

int
main(int argc, char *argv[])
{
  uint32_t nPackets = 5;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue("nPackets", "Number of ICMPv6 Echo Requests to send", nPackets);
  cmd.AddValue("interval", "Interval between Echo Requests (s)", interval);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize("100p")));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall(false);
  internetv6.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
  interfaces.SetForwarding(0, true);
  interfaces.SetDefaultRouteInAllNodes(0);

  Ptr<NetDevice> nd0 = devices.Get(0);
  Ptr<NetDevice> nd1 = devices.Get(1);

  Ptr<Queue<Packet>> dtq0 = DynamicCast<PointToPointNetDevice>(nd0) ?
                            0 :
                            DynamicCast<CsmaNetDevice>(nd0)->GetQueue();
  Ptr<Queue<Packet>> dtq1 = DynamicCast<PointToPointNetDevice>(nd1) ?
                            0 :
                            DynamicCast<CsmaNetDevice>(nd1)->GetQueue();

  if (dtq0)
    dtq0->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueTrace));
  if (dtq1)
    dtq1->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueTrace));

  nd1->TraceConnectWithoutContext("MacRx", MakeCallback(&PacketReceiveTrace));
  nd0->TraceConnectWithoutContext("MacRx", MakeCallback(&PacketReceiveTrace));

  uint16_t echoPort = 9;

  Icmpv6EchoServerHelper echoServerHelper(echoPort);
  ApplicationContainer serverApps = echoServerHelper.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(30.0));

  Icmpv6EchoClientHelper echoClientHelper(interfaces.GetAddress(1,1), echoPort);
  echoClientHelper.SetAttribute("MaxPackets", UintegerValue(nPackets));
  echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(interval)));
  echoClientHelper.SetAttribute("PacketSize", UintegerValue(56));

  ApplicationContainer clientApps = echoClientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(30.0));

  Simulator::Stop(Seconds(31.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}