#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/multicast-routing-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MulticastStaticRoutingExample");

static uint32_t totalRx = 0;

void RxTrace (Ptr<const Packet> p, const Address &a)
{
  totalRx += p->GetSize ();
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes: A (0), B (1), C (2), D (3), E (4)
  NodeContainer nodes;
  nodes.Create (5);

  // Define the topology: create p2p links manually
  // We'll use this mapping:
  // Connect A-B, A-C, B-D, C-E (but don't directly connect A-D, A-E ...)
  // Let's blacklist direct link from A to D, A to E, B to E, C to D, etc.

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // A-B
  NetDeviceContainer nd0 = p2p.Install (nodes.Get(0), nodes.Get(1));
  // A-C
  NetDeviceContainer nd1 = p2p.Install (nodes.Get(0), nodes.Get(2));
  // B-D
  NetDeviceContainer nd2 = p2p.Install (nodes.Get(1), nodes.Get(3));
  // C-E
  NetDeviceContainer nd3 = p2p.Install (nodes.Get(2), nodes.Get(4));

  // Install Internet stack with static multicast routing
  InternetStackHelper internet;
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (Ipv4GlobalRoutingHelper(), 10); // fallback
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  // Assign IP addresses per interface
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ab = ipv4.Assign (nd0);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ac = ipv4.Assign (nd1);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_bd = ipv4.Assign (nd2);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ce = ipv4.Assign (nd3);

  // Select a multicast group address
  Ipv4Address multicastGroup ("225.1.2.4");
  uint16_t multicastPort = 5000;

  // Configure static multicast routing for all nodes
  // A is source, B/C/D/E are receivers
  // -- At sender (A) --
  Ptr<Node> A = nodes.Get(0);
  Ptr<NetDevice> A_ifAB = nd0.Get(0);
  Ptr<NetDevice> A_ifAC = nd1.Get(0);
  // -- At B --
  Ptr<Node> B = nodes.Get(1);
  Ptr<NetDevice> B_ifAB = nd0.Get(1);
  Ptr<NetDevice> B_ifBD = nd2.Get(0);
  // -- At C --
  Ptr<Node> C = nodes.Get(2);
  Ptr<NetDevice> C_ifAC = nd1.Get(1);
  Ptr<NetDevice> C_ifCE = nd3.Get(0);
  // -- At D --
  Ptr<Node> D = nodes.Get(3);
  Ptr<NetDevice> D_ifBD = nd2.Get(1);
  // -- At E --
  Ptr<Node> E = nodes.Get(4);
  Ptr<NetDevice> E_ifCE = nd3.Get(1);

  // All nodes join multicast group (except source for receiving)
  // (B, C, D, E join group)
  Ptr<Ipv4> ipv4_B = B->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4_C = C->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4_D = D->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4_E = E->GetObject<Ipv4>();

  ipv4_B->JoinMulticastGroup (1, multicastGroup); // 1 = ifIndex for AB
  ipv4_C->JoinMulticastGroup (1, multicastGroup); // 1 = ifIndex for AC
  ipv4_D->JoinMulticastGroup (1, multicastGroup); // 1 = ifIndex for BD
  ipv4_E->JoinMulticastGroup (1, multicastGroup); // 1 = ifIndex for CE

  // Setup multicast static routing
  Ptr<Ipv4StaticRouting> A_static = staticRouting.GetStaticRouting (A->GetObject<Ipv4>());
  Ptr<Ipv4StaticRouting> B_static = staticRouting.GetStaticRouting (B->GetObject<Ipv4>());
  Ptr<Ipv4StaticRouting> C_static = staticRouting.GetStaticRouting (C->GetObject<Ipv4>());
  Ptr<Ipv4StaticRouting> D_static = staticRouting.GetStaticRouting (D->GetObject<Ipv4>());
  Ptr<Ipv4StaticRouting> E_static = staticRouting.GetStaticRouting (E->GetObject<Ipv4>());

  // A forwards to both AB and AC, as root
  std::vector<NetDevice*> outIf_A;
  outIf_A.push_back (A_ifAB);
  outIf_A.push_back (A_ifAC);
  A_static->AddMulticastRoute (0, multicastGroup, outIf_A);

  // B forwards to BD for multicast group
  std::vector<NetDevice*> outIf_B;
  outIf_B.push_back (B_ifBD);
  B_static->AddMulticastRoute (B_ifAB, multicastGroup, outIf_B);

  // C forwards to CE for multicast group
  std::vector<NetDevice*> outIf_C;
  outIf_C.push_back (C_ifCE);
  C_static->AddMulticastRoute (C_ifAC, multicastGroup, outIf_C);

  // D/E are leaves; no further forwarding needed

  // Sink applications for B, C, D, E
  ApplicationContainer sinks;
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (multicastGroup, multicastPort));

  sinks.Add (sinkHelper.Install (B));
  sinks.Add (sinkHelper.Install (C));
  sinks.Add (sinkHelper.Install (D));
  sinks.Add (sinkHelper.Install (E));

  sinks.Start (Seconds (0.0));
  sinks.Stop (Seconds (10.0));

  // Attach packet receive trace for stats
  for (uint32_t i = 0; i < sinks.GetN (); ++i)
  {
    Ptr<Application> app = sinks.Get (i);
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
    sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));
  }

  // Application on A: OnOff sends to multicast address:port
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (multicastGroup, multicastPort));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("DataRate", StringValue ("2Mbps"));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

  ApplicationContainer srcApp = onoff.Install (A);

  // Enable pcap tracing everywhere
  p2p.EnablePcapAll ("multicast-static-routing");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  std::cout << "Total bytes received by sinks: " << totalRx << std::endl;

  Simulator::Destroy ();
  return 0;
}