#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-rip.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipSimpleRouting");

class PrintRipTable : public Object
{
public:
  static TypeId GetTypeId(void)
  {
    return TypeId("PrintRipTable").SetParent<Object>().AddConstructor<PrintRipTable>();
  }

  void PrintTables(Ptr<Node> node, uint32_t nodeid)
  {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
    if (rp == nullptr)
    {
      NS_LOG_WARN("No routing protocol for node " << nodeid);
      return;
    }

    Ipv4RoutingTableEntry entry;
    std::ostringstream ost;
    ost << "*** Routing Table on Node: " << nodeid << " ***";
    NS_LOG_INFO(ost.str());

    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
    {
      for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j)
      {
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(i, j);
        NS_LOG_INFO("Interface: " << i << " Address: " << iaddr.GetLocal());
      }
    }

    for (uint32_t k = 0; k < rp->GetNRoutes(); ++k)
    {
      entry = rp->GetRoute(k);
      NS_LOG_INFO("Destination: " << entry.GetDest() << " Mask: " << entry.GetMask()
                                  << " Gateway: " << entry.GetGateway() << " Interface: " << entry.GetInterface());
    }
  }
};

int main(int argc, char *argv[])
{
  bool verbose = false;
  bool printRoutingTable = true;
  bool showPings = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("verbose", "Turn on log components", verbose);
  cmd.AddValue("printRoutingTable", "Print routing tables at intervals", printRoutingTable);
  cmd.AddValue("showPings", "Show ping RTTs.", showPings);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4Rip", LOG_LEVEL_ALL);
  }

  // Create nodes
  Ptr<Node> src = CreateObject<Node>();
  Ptr<Node> dst = CreateObject<Node>();
  Names::Add("SRC", src);
  Names::Add("DST", dst);

  Ptr<Node> A = CreateObject<Node>();
  Ptr<Node> B = CreateObject<Node>();
  Ptr<Node> C = CreateObject<Node>();
  Ptr<Node> D = CreateObject<Node>();
  Names::Add("A", A);
  Names::Add("B", B);
  Names::Add("C", C);
  Names::Add("D", D);

  NodeContainer nodes;
  nodes.Add(src);
  nodes.Add(dst);
  nodes.Add(A);
  nodes.Add(B);
  nodes.Add(C);
  nodes.Add(D);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devSrcA, devAB, devBD, devDC, devCA, devAD;

  devSrcA = p2p.Install(src, A);
  devAB = p2p.Install(A, B);
  devBD = p2p.Install(B, D);
  devDC = p2p.Install(D, C);
  devCA = p2p.Install(C, A);
  devAD = p2p.Install(A, D);

  InternetStackHelper internet;
  RipHelper ripRouting;

  ripRouting.SetAttribute("SplitHorizon", EnumValue(Ipv4Rip::POISON_REVERSE));

  Ipv4ListRoutingHelper listRH;
  listRH.Add(ripRouting, 0);

  InternetStackHelper stack;
  stack.SetRoutingHelper(listRH);
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  ipv4.Assign(devSrcA);
  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  ipv4.Assign(devAB);
  ipv4.SetBase("10.0.2.0", "255.255.255.0");
  ipv4.Assign(devBD);
  ipv4.SetBase("10.0.3.0", "255.255.255.0");
  ipv4.Assign(devDC);
  ipv4.SetBase("10.0.4.0", "255.255.255.0");
  ipv4.Assign(devCA);
  ipv4.SetBase("10.0.5.0", "255.255.255.0");
  ipv4.Assign(devAD);

  // Set link costs
  ripRouting.ExcludeLink(B, D, Seconds(10)); // cost of 10
  ripRouting.ExcludeLink(C, D, Seconds(10));

  // Setup sink and source applications
  uint32_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.0.0.2"), port)));
  onoff.SetConstantRate(DataRate("5kbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1000));

  ApplicationContainer app = onoff.Install(src);
  app.Start(Seconds(1.0));
  app.Stop(Seconds(60.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  app = sink.Install(dst);
  app.Start(Seconds(0.0));

  Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  V4PingHelper ping(dstAddr);
  ping.SetAttribute("Verbose", BooleanValue(showPings));
  ApplicationContainer pingApp = ping.Install(src);
  pingApp.Start(Seconds(0.0));
  pingApp.Stop(Seconds(60.0));

  // Schedule link failure and recovery
  Simulator::Schedule(Seconds(40.0), &RipHelper::InjectLinkBreak, &ripRouting, B, D);
  Simulator::Schedule(Seconds(44.0), &RipHelper::RecoverLink, &ripRouting, B, D);

  // Schedule routing table printing
  if (printRoutingTable)
  {
    PrintRipTable prt;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Simulator::ScheduleNow(&PrintRipTable::PrintTables, &prt, nodes.Get(i), i);
      Simulator::Schedule(Seconds(10.0), &PrintRipTable::PrintTables, &prt, nodes.Get(i), i);
      Simulator::Schedule(Seconds(20.0), &PrintRipTable::PrintTables, &prt, nodes.Get(i), i);
      Simulator::Schedule(Seconds(30.0), &PrintRipTable::PrintTables, &prt, nodes.Get(i), i);
      Simulator::Schedule(Seconds(40.0), &PrintRipTable::PrintTables, &prt, nodes.Get(i), i);
      Simulator::Schedule(Seconds(50.0), &PrintRipTable::PrintTables, &prt, nodes.Get(i), i);
    }
  }

  Simulator::Stop(Seconds(60.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}