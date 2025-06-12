#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologySimulation");

void
TxTrace(std::string context, Ptr<const Packet> packet)
{
  NS_LOG_INFO("Packet transmitted at " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize());
}

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO("Packet received at " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize());
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("TreeTopologySimulation", LOG_LEVEL_INFO);

  // Create four nodes (root: n0, child1: n1, child2: n2, leaf: n3)
  NodeContainer n0, n1, n2, n3;
  n0.Create(1); // root
  n1.Create(1); // child1
  n2.Create(1); // child2
  n3.Create(1); // leaf

  // Connect n0<->n1, n0<->n2, n2<->n3
  NodeContainer n0n1(n0.Get(0), n1.Get(0));
  NodeContainer n0n2(n0.Get(0), n2.Get(0));
  NodeContainer n2n3(n2.Get(0), n3.Get(0));

  // Point-to-Point channel attributes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install devices and channels
  NetDeviceContainer d0d1 = p2p.Install(n0n1);
  NetDeviceContainer d0d2 = p2p.Install(n0n2);
  NetDeviceContainer d2d3 = p2p.Install(n2n3);

  // Install Internet Stack
  NodeContainer allNodes;
  allNodes.Add(n0);
  allNodes.Add(n1);
  allNodes.Add(n2);
  allNodes.Add(n3);
  InternetStackHelper internet;
  internet.Install(allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if0_1 = address.Assign(d0d1);

  address.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if0_2 = address.Assign(d0d2);

  address.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if2_3 = address.Assign(d2d3);

  // Set up static routing
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ptr<Ipv4StaticRouting> n3Routing = staticRoutingHelper.GetStaticRouting(n3.Get(0)->GetObject<Ipv4>());
  n3Routing->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), if2_3.GetAddress(0), 1);
  n3Routing->SetDefaultRoute(if2_3.GetAddress(0), 1);

  Ptr<Ipv4StaticRouting> n2Routing = staticRoutingHelper.GetStaticRouting(n2.Get(0)->GetObject<Ipv4>());
  n2Routing->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), if0_2.GetAddress(0), 1);
  n2Routing->AddNetworkRouteTo(Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"), if2_3.GetAddress(1), 2);

  Ptr<Ipv4StaticRouting> n1Routing = staticRoutingHelper.GetStaticRouting(n1.Get(0)->GetObject<Ipv4>());
  n1Routing->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), if0_1.GetAddress(0), 1);
  n1Routing->AddNetworkRouteTo(Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"), if0_1.GetAddress(0), 1);

  // Root node knows how to reach others
  Ptr<Ipv4StaticRouting> n0Routing = staticRoutingHelper.GetStaticRouting(n0.Get(0)->GetObject<Ipv4>());
  n0Routing->AddNetworkRouteTo(Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"), if0_2.GetAddress(1), 2);
  n0Routing->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), if0_2.GetAddress(1), 2);
  n0Routing->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), if0_1.GetAddress(1), 1);

  // Set up UDP EchoServer on root (n0)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(n0.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Set up UDP EchoClient on leaf (n3)
  UdpEchoClientHelper echoClient(if0_1.GetAddress(0), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(n3.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Packet transmission/reception tracing
  Config::Connect("/NodeList/3/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback(&TxTrace));
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback(&RxTrace));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}