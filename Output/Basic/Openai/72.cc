#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("L2MultiswitchLanWithWan");

int
main (int argc, char *argv[])
{
  std::string lanRate = "100Mbps";
  std::string lanDelay = "6560ns"; // default for 100Mbps in ns-3 examples
  std::string wanRate = "10Mbps";
  std::string wanDelay = "30ms";

  CommandLine cmd;
  cmd.AddValue ("lanRate", "LAN Ethernet rate (e.g. 100Mbps or 10Mbps)", lanRate);
  cmd.AddValue ("lanDelay", "LAN Ethernet delay", lanDelay);
  cmd.AddValue ("wanRate", "WAN link speed (e.g. 10Mbps)", wanRate);
  cmd.AddValue ("wanDelay", "WAN propagation delay (e.g. 30ms)", wanDelay);
  cmd.Parse (argc, argv);

  // ========== NODES ==========

  // Top LAN: t2 (client), t3 (server), switches, router (tr)
  Ptr<Node> t2 = CreateObject<Node> ();
  Ptr<Node> t3 = CreateObject<Node> ();
  Ptr<Node> tr = CreateObject<Node> ();
  Ptr<Node> tsw1 = CreateObject<Node> ();
  Ptr<Node> tsw2 = CreateObject<Node> ();

  // Bottom LAN: b2 (server), b3 (client), switches, router (br)
  Ptr<Node> b2 = CreateObject<Node> ();
  Ptr<Node> b3 = CreateObject<Node> ();
  Ptr<Node> br = CreateObject<Node> ();
  Ptr<Node> bsw1 = CreateObject<Node> ();
  Ptr<Node> bsw2 = CreateObject<Node> ();

  NodeContainer topLanNodes, topSwitches, topRouter;
  NodeContainer bottomLanNodes, bottomSwitches, bottomRouter;

  topLanNodes.Add (t2);
  topLanNodes.Add (t3);
  topSwitches.Add (tsw1);
  topSwitches.Add (tsw2);
  topRouter.Add (tr);

  bottomLanNodes.Add (b2);
  bottomLanNodes.Add (b3);
  bottomSwitches.Add (bsw1);
  bottomSwitches.Add (bsw2);
  bottomRouter.Add (br);

  // ========== CSMA HELPERS ==========

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue (lanRate));
  csma.SetChannelAttribute ("Delay", StringValue (lanDelay));

  // ====== TOPOLOGY: Top LAN ======

  // t2 <-> tsw1 <-> tsw2 <-> tr
  NetDeviceContainer t2_s1 = csma.Install (NodeContainer (t2, tsw1));
  NetDeviceContainer s1_s2 = csma.Install (NodeContainer (tsw1, tsw2));
  NetDeviceContainer s2_tr = csma.Install (NodeContainer (tsw2, tr));

  // t3 <-> tsw1 (single-switch path)
  NetDeviceContainer t3_s1 = csma.Install (NodeContainer (t3, tsw1));

  // Set up bridge devices for top switches
  BridgeHelper bridge;
  NetDeviceContainer tsw1_ports;
  tsw1_ports.Add (t2_s1.Get (1));  // tsw1 port to t2
  tsw1_ports.Add (s1_s2.Get (0));  // tsw1 port to tsw2
  tsw1_ports.Add (t3_s1.Get (1));  // tsw1 port to t3
  bridge.Install (tsw1, tsw1_ports);

  NetDeviceContainer tsw2_ports;
  tsw2_ports.Add (s1_s2.Get (1));  // tsw2 port to tsw1
  tsw2_ports.Add (s2_tr.Get (0));  // tsw2 port to tr
  bridge.Install (tsw2, tsw2_ports);

  // =========== Bottom LAN ===========

  // b2 <-> bsw1 <-> bsw2 <-> br
  NetDeviceContainer b2_s1 = csma.Install (NodeContainer (b2, bsw1));
  NetDeviceContainer s1_s2_b = csma.Install (NodeContainer (bsw1, bsw2));
  NetDeviceContainer s2_br = csma.Install (NodeContainer (bsw2, br));

  // b3 <-> bsw1 (single-switch path)
  NetDeviceContainer b3_s1 = csma.Install (NodeContainer (b3, bsw1));

  // Set up bridge devices for bottom switches
  NetDeviceContainer bsw1_ports;
  bsw1_ports.Add (b2_s1.Get (1));   // bsw1 port to b2
  bsw1_ports.Add (s1_s2_b.Get (0)); // bsw1 port to bsw2
  bsw1_ports.Add (b3_s1.Get (1));   // bsw1 port to b3
  bridge.Install (bsw1, bsw1_ports);

  NetDeviceContainer bsw2_ports;
  bsw2_ports.Add (s1_s2_b.Get (1)); // bsw2 port to bsw1
  bsw2_ports.Add (s2_br.Get (0));   // bsw2 port to br
  bridge.Install (bsw2, bsw2_ports);

  // ========== WAN LINK ==========
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (wanRate));
  p2p.SetChannelAttribute ("Delay", StringValue (wanDelay));
  NetDeviceContainer wanDevs = p2p.Install (NodeContainer (tr, br));

  // ========== INTERNET STACK ==========

  InternetStackHelper stack;
  stack.Install (t2);
  stack.Install (t3);
  stack.Install (b2);
  stack.Install (b3);
  stack.Install (tr);
  stack.Install (br);

  // ========== ADDRESSING ==========

  Ipv4AddressHelper topLanAddress, bottomLanAddress, wanAddress;
  topLanAddress.SetBase ("192.168.1.0", "255.255.255.0");
  bottomLanAddress.SetBase ("192.168.2.0", "255.255.255.0");
  wanAddress.SetBase ("10.10.10.0", "255.255.255.0");

  // Assign addresses:

  // Top LAN assignments: t2, t3, tr
  // Top LAN: t2 (0), t3 (1), tr (through s2, get 2)
  NetDeviceContainer topLanEndDevices;
  topLanEndDevices.Add (t2_s1.Get (0));      // t2 Ethernet
  topLanEndDevices.Add (t3_s1.Get (0));      // t3 Ethernet
  topLanEndDevices.Add (s2_tr.Get (1));      // tr's LAN port
  Ipv4InterfaceContainer topLanIfs = topLanAddress.Assign (topLanEndDevices);

  // Bottom LAN assignments: b2, b3, br
  NetDeviceContainer bottomLanEndDevices;
  bottomLanEndDevices.Add (b2_s1.Get (0));       // b2 Ethernet
  bottomLanEndDevices.Add (b3_s1.Get (0));       // b3 Ethernet
  bottomLanEndDevices.Add (s2_br.Get (1));       // br's LAN port
  Ipv4InterfaceContainer bottomLanIfs = bottomLanAddress.Assign (bottomLanEndDevices);

  // WAN assignments: tr, br p2p link (assign after LAN to avoid address clash)
  Ipv4InterfaceContainer wanIfs = wanAddress.Assign (wanDevs);

  // ========== ROUTING ==========

  // Enable forwarding on routers
  Ptr<Ipv4> trIpv4 = tr->GetObject<Ipv4> ();
  trIpv4->SetAttribute ("IpForward", BooleanValue (true));
  Ptr<Ipv4> brIpv4 = br->GetObject<Ipv4> ();
  brIpv4->SetAttribute ("IpForward", BooleanValue (true));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // ========== UDP ECHO ==========
  
  // Top: t3 is UDP Echo Server, t2 is UDP Echo Client
  uint16_t echoPort1 = 9;
  UdpEchoServerHelper echoServer1 (echoPort1);
  ApplicationContainer serverApps1 = echoServer1.Install (t3);
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient1 (topLanIfs.GetAddress (1), echoPort1); // t3 address
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps1 = echoClient1.Install (t2);
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (20.0));

  // Bottom: b2 is UDP Echo Server, b3 is UDP Echo Client
  uint16_t echoPort2 = 13;
  UdpEchoServerHelper echoServer2 (echoPort2);
  ApplicationContainer serverApps2 = echoServer2.Install (b2);
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient2 (bottomLanIfs.GetAddress (0), echoPort2); // b2 address
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps2 = echoClient2.Install (b3);
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (20.0));

  Simulator::Stop (Seconds (22.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}