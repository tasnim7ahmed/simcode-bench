#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-distance-vector-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SplitHorizonDVExample");

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  LogComponentEnable ("DistanceVectorRoutingProtocol", LOG_LEVEL_INFO);

  // Create three nodes: A, B, C
  NodeContainer nodes;
  nodes.Create(3);

  // Create point-to-point channels: A-B, B-C
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d0d1 = p2p.Install(n0n1);
  NetDeviceContainer d1d2 = p2p.Install(n1n2);

  // Install Internet Stack with Distance Vector Routing (split horizon enabled by default)
  InternetStackHelper stack;
  DistanceVectorRoutingHelper dvHelper;
  Ipv4ListRoutingHelper list;
  list.Add (dvHelper, 10);

  stack.SetRoutingHelper(list);
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper addr;
  addr.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if0 = addr.Assign(d0d1);

  addr.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if1 = addr.Assign(d1d2);

  // Install UDP Echo Server on node C (2)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(12.0));

  // Install UDP Echo Client on node A (0)
  UdpEchoClientHelper echoClient(if1.GetAddress(1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(64));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(11.0));

  // Enable routing table logging to verify split horizon effect
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      Simulator::Schedule(Seconds(3.0), &Ipv4GlobalRoutingHelper::PrintRoutingTable, node);
      Simulator::Schedule(Seconds(7.0), &Ipv4GlobalRoutingHelper::PrintRoutingTable, node);
      Simulator::Schedule(Seconds(11.0), &Ipv4GlobalRoutingHelper::PrintRoutingTable, node);
    }

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}