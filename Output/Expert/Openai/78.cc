#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/rip-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <vector>
#include <iostream>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipTopologyExample");

void
PrintRoutingTables(Ptr<OutputStreamWrapper> stream, std::vector<Ptr<Node>> routers)
{
  for (uint32_t i = 0; i < routers.size(); ++i)
  {
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4> ipv4 = routers[i]->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> routingProt = ipv4->GetRoutingProtocol();
    routingProt->PrintRoutingTable(stream);
    *stream->GetStream() << std::endl;
  }
}

void
DisableLink(Ptr<NetDevice> dev1, Ptr<NetDevice> dev2)
{
  dev1->SetAttribute("ReceiveErrorModel", PointerValue(CreateObject<RateErrorModel>()));
  dev2->SetAttribute("ReceiveErrorModel", PointerValue(CreateObject<RateErrorModel>()));
}

void
EnableLink(Ptr<NetDevice> dev1, Ptr<NetDevice> dev2)
{
  dev1->SetAttribute("ReceiveErrorModel", PointerValue(0));
  dev2->SetAttribute("ReceiveErrorModel", PointerValue(0));
}

int
main(int argc, char *argv[])
{
  bool verbose = false;
  bool printRt = false;
  bool showPing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Enable device/stack logging", verbose);
  cmd.AddValue("printRt", "Print routing tables at intervals", printRt);
  cmd.AddValue("showPing", "Show ICMP ping echo responses", showPing);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("RipTopologyExample", LOG_LEVEL_INFO);
    LogComponentEnable("Rip", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
    LogComponentEnable("Ping6Application", LOG_LEVEL_ALL);
    LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
  }

  NodeContainer nodes;
  nodes.Create(6); // 0: SRC, 1: DST, 2: A, 3: B, 4: C, 5: D
  Ptr<Node> nodeSrc = nodes.Get(0);
  Ptr<Node> nodeDst = nodes.Get(1);
  Ptr<Node> nodeA = nodes.Get(2);
  Ptr<Node> nodeB = nodes.Get(3);
  Ptr<Node> nodeC = nodes.Get(4);
  Ptr<Node> nodeD = nodes.Get(5);

  // Link pairs for clarity
  // 0: SRC <-> A
  // 1: A   <-> B
  // 2: B   <-> D
  // 3: D   <-> C (cost=10)
  // 4: C   <-> A
  // 5: D   <-> DST

  std::vector<NodeContainer> linkNodes = {
    NodeContainer(nodeSrc, nodeA),
    NodeContainer(nodeA, nodeB),
    NodeContainer(nodeB, nodeD),
    NodeContainer(nodeD, nodeC),
    NodeContainer(nodeC, nodeA),
    NodeContainer(nodeD, nodeDst)
  };

  std::vector<PointToPointHelper> p2pLinks(6);
  for (uint32_t i = 0; i < 6; ++i)
  {
    p2pLinks[i].SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLinks[i].SetChannelAttribute("Delay", StringValue("2ms"));
  }

  // Set high metric (=10) for D-C
  p2pLinks[3].SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2pLinks[3].SetChannelAttribute("Delay", StringValue("2ms"));

  std::vector<NetDeviceContainer> devices(6);
  for (uint32_t i = 0; i < 6; ++i)
    devices[i] = p2pLinks[i].Install(linkNodes[i]);

  InternetStackHelper stack;
  RipHelper rip;

  rip.ExcludeInterface(nodeSrc, 1); // Do not run routing on hosts
  rip.ExcludeInterface(nodeDst, 1);

  rip.Set("SplitHorizon", EnumValue(RipHelper::POISON_REVERSE));

  stack.SetRoutingHelper(rip);
  stack.Install(NodeContainer(nodeA, nodeB, nodeC, nodeD));

  InternetStackHelper srcDstStack;
  srcDstStack.Install(NodeContainer(nodeSrc, nodeDst));

  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> ifaces(6);

  ipv4.SetBase("10.0.1.0", "255.255.255.0"); // SRC <-> A
  ifaces[0] = ipv4.Assign(devices[0]);
  ipv4.NewNetwork();
  ifaces[1] = ipv4.Assign(devices[1]); // A <-> B
  ipv4.NewNetwork();
  ifaces[2] = ipv4.Assign(devices[2]); // B <-> D
  ipv4.NewNetwork();
  ifaces[3] = ipv4.Assign(devices[3]); // D <-> C (cost=10)
  ipv4.NewNetwork();
  ifaces[4] = ipv4.Assign(devices[4]); // C <-> A
  ipv4.NewNetwork();
  ifaces[5] = ipv4.Assign(devices[5]); // D <-> DST

  // Manually set RIP metric for C-D link
  Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4D = nodeD->GetObject<Ipv4>();
  for (uint32_t i = 0; i < ipv4C->GetNInterfaces(); ++i)
    if (ipv4C->GetAddress(i, 0).GetLocal() == ifaces[3].GetAddress(1)) // C side of D<->C
      ipv4C->SetMetric(i, 10);
  for (uint32_t i = 0; i < ipv4D->GetNInterfaces(); ++i)
    if (ipv4D->GetAddress(i, 0).GetLocal() == ifaces[3].GetAddress(0)) // D side of D<->C
      ipv4D->SetMetric(i, 10);

  Ipv4Address srcAddr = ifaces[0].GetAddress(0);
  Ipv4Address dstAddr = ifaces[5].GetAddress(1);

  // Add static routes on source and destination to reach the network
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> srcStaticRouting = staticRouting.GetStaticRouting(nodeSrc->GetObject<Ipv4>());
  Ptr<Ipv4StaticRouting> dstStaticRouting = staticRouting.GetStaticRouting(nodeDst->GetObject<Ipv4>());
  srcStaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.6.0"), Ipv4Mask("255.255.255.0"), ifaces[0].GetAddress(1), 1);
  dstStaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), ifaces[5].GetAddress(0), 1);

  // Ping application
  uint16_t packetSize = 56;
  uint32_t maxPackets = 10e6;
  Time interPacketInterval = Seconds(1.0);
  V4PingHelper ping(dstAddr);
  ping.SetAttribute("Interval", TimeValue(interPacketInterval));
  ping.SetAttribute("Size", UintegerValue(packetSize));
  ping.SetAttribute("Verbose", BooleanValue(showPing));
  ping.SetAttribute("Count", UintegerValue(maxPackets));
  ApplicationContainer pingApps = ping.Install(nodeSrc);
  pingApps.Start(Seconds(2.0));
  pingApps.Stop(Seconds(60.0));

  // Schedule link down (B<->D) at 40s and up at 44s
  Ptr<NetDevice> b2 = devices[2].Get(0); // B's net device to D
  Ptr<NetDevice> d2 = devices[2].Get(1); // D's net device to B

  Simulator::Schedule(Seconds(40.0), &DisableLink, b2, d2);
  Simulator::Schedule(Seconds(44.0), &EnableLink, b2, d2);

  // Routetable printing schedule
  if (printRt)
  {
    std::vector<Ptr<Node>> routerNodes = {nodeA, nodeB, nodeC, nodeD};
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    for (double t = 0.0; t < 60.1; t += 5.0)
    {
      Simulator::Schedule(Seconds(t), &PrintRoutingTables, routingStream, routerNodes);
    }
  }

  Simulator::Stop(Seconds(60.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}