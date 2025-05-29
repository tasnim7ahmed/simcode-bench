#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LanWanLanBridgedRoutersExample");

int
main (int argc, char *argv[])
{
  std::string csmaDataRate = "100Mbps";
  std::string csmaDelay = "6560ns";

  CommandLine cmd;
  cmd.AddValue ("csmaDataRate", "Data rate for CSMA LAN links", csmaDataRate);
  cmd.Parse (argc, argv);

  // Top LAN Devices/Nodes: switches ts1-ts4, endpoints t2, t3
  // Bottom LAN Devices/Nodes: switches bs1-bs5, endpoints b2, b3
  // Routers: tr (top), br (bottom)
  // WAN: p2p tr-br

  // Node creation
  Ptr<Node> tr = CreateObject<Node> ();
  Ptr<Node> br = CreateObject<Node> ();
  NodeContainer topSwitches;
  topSwitches.Create (4); // ts1, ts2, ts3, ts4
  Ptr<Node> t2 = CreateObject<Node> ();
  Ptr<Node> t3 = CreateObject<Node> ();
  NodeContainer bottomSwitches;
  bottomSwitches.Create (5); // bs1-bs5
  Ptr<Node> b2 = CreateObject<Node> ();
  Ptr<Node> b3 = CreateObject<Node> ();

  // Node indices for easier mapping
  Ptr<Node> ts1 = topSwitches.Get (0);
  Ptr<Node> ts2 = topSwitches.Get (1);
  Ptr<Node> ts3 = topSwitches.Get (2);
  Ptr<Node> ts4 = topSwitches.Get (3);

  Ptr<Node> bs1 = bottomSwitches.Get (0);
  Ptr<Node> bs2 = bottomSwitches.Get (1);
  Ptr<Node> bs3 = bottomSwitches.Get (2);
  Ptr<Node> bs4 = bottomSwitches.Get (3);
  Ptr<Node> bs5 = bottomSwitches.Get (4);

  // CSMA for Top LAN
  CsmaHelper csmaTop;
  csmaTop.SetChannelAttribute ("DataRate", StringValue (csmaDataRate));
  csmaTop.SetChannelAttribute ("Delay", StringValue (csmaDelay));

  // Top LAN switch interconnections:
  // (ts1)-(ts2)-(ts3)-(ts4)
  // t2 connects to ts2, t3 connects to ts4, tr connects to ts1
  NodeContainer ts1ts2; ts1ts2.Add (ts1); ts1ts2.Add (ts2);
  NodeContainer ts2ts3; ts2ts3.Add (ts2); ts2ts3.Add (ts3);
  NodeContainer ts3ts4; ts3ts4.Add (ts3); ts3ts4.Add (ts4);

  // Links
  NetDeviceContainer nd_ts1ts2 = csmaTop.Install (ts1ts2);
  NetDeviceContainer nd_ts2ts3 = csmaTop.Install (ts2ts3);
  NetDeviceContainer nd_ts3ts4 = csmaTop.Install (ts3ts4);

  NetDeviceContainer nd_trts1 = csmaTop.Install (NodeContainer (tr, ts1));
  NetDeviceContainer nd_t2ts2 = csmaTop.Install (NodeContainer (t2, ts2));
  NetDeviceContainer nd_t3ts4 = csmaTop.Install (NodeContainer (t3, ts4));

  // Bridge devices per switch
  // ts1: bridge nd_ts1ts2(0), nd_trts1(1)
  BridgeHelper bridge;
  NetDeviceContainer bdev_ts1;
  bdev_ts1 = bridge.Install (ts1, NetDeviceContainer (nd_ts1ts2.Get (0), nd_trts1.Get (1)));
  // ts2: bridge nd_ts1ts2(1), nd_ts2ts3(0), nd_t2ts2(1)
  NetDeviceContainer bdev_ts2;
  bdev_ts2 = bridge.Install (ts2, NetDeviceContainer (nd_ts1ts2.Get (1), nd_ts2ts3.Get (0), nd_t2ts2.Get (1)));
  // ts3: bridge nd_ts2ts3(1), nd_ts3ts4(0)
  NetDeviceContainer bdev_ts3;
  bdev_ts3 = bridge.Install (ts3, NetDeviceContainer (nd_ts2ts3.Get (1), nd_ts3ts4.Get (0)));
  // ts4: bridge nd_ts3ts4(1), nd_t3ts4(1)
  NetDeviceContainer bdev_ts4;
  bdev_ts4 = bridge.Install (ts4, NetDeviceContainer (nd_ts3ts4.Get (1), nd_t3ts4.Get (1)));

  // Top LAN - collect all interfaces for IP stack assignment
  std::vector<Ptr<Node>> topLanLeaves = {t2, t3};

  // CSMA for Bottom LAN
  CsmaHelper csmaBottom;
  csmaBottom.SetChannelAttribute ("DataRate", StringValue (csmaDataRate));
  csmaBottom.SetChannelAttribute ("Delay", StringValue (csmaDelay));

  // Bottom LAN interconnections
  // (bs1)-(bs2)-(bs3)-(bs4)-(bs5)
  // b2 attaches to bs2, b3 attaches to bs5, br attaches to bs1
  NodeContainer bs1bs2; bs1bs2.Add (bs1); bs1bs2.Add (bs2);
  NodeContainer bs2bs3; bs2bs3.Add (bs2); bs2bs3.Add (bs3);
  NodeContainer bs3bs4; bs3bs4.Add (bs3); bs3bs4.Add (bs4);
  NodeContainer bs4bs5; bs4bs5.Add (bs4); bs4bs5.Add (bs5);

  NetDeviceContainer nd_bs1bs2 = csmaBottom.Install (bs1bs2);
  NetDeviceContainer nd_bs2bs3 = csmaBottom.Install (bs2bs3);
  NetDeviceContainer nd_bs3bs4 = csmaBottom.Install (bs3bs4);
  NetDeviceContainer nd_bs4bs5 = csmaBottom.Install (bs4bs5);

  NetDeviceContainer nd_brbs1 = csmaBottom.Install (NodeContainer (br, bs1));
  NetDeviceContainer nd_b2bs2 = csmaBottom.Install (NodeContainer (b2, bs2));
  NetDeviceContainer nd_b3bs5 = csmaBottom.Install (NodeContainer (b3, bs5));

  // Bridge devices per switch
  NetDeviceContainer bdev_bs1;
  bdev_bs1 = bridge.Install (bs1, NetDeviceContainer (nd_bs1bs2.Get (0), nd_brbs1.Get (1)));
  NetDeviceContainer bdev_bs2;
  bdev_bs2 = bridge.Install (bs2, NetDeviceContainer (nd_bs1bs2.Get (1), nd_bs2bs3.Get (0), nd_b2bs2.Get (1)));
  NetDeviceContainer bdev_bs3;
  bdev_bs3 = bridge.Install (bs3, NetDeviceContainer (nd_bs2bs3.Get (1), nd_bs3bs4.Get (0)));
  NetDeviceContainer bdev_bs4;
  bdev_bs4 = bridge.Install (bs4, NetDeviceContainer (nd_bs3bs4.Get (1), nd_bs4bs5.Get (0)));
  NetDeviceContainer bdev_bs5;
  bdev_bs5 = bridge.Install (bs5, NetDeviceContainer (nd_bs4bs5.Get (1), nd_b3bs5.Get (1)));

  std::vector<Ptr<Node>> bottomLanLeaves = {b2, b3};

  // WAN: P2P link between tr and br
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));

  NetDeviceContainer nd_trbr = p2p.Install (tr, br);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (tr);
  stack.Install (br);
  stack.Install (t2);
  stack.Install (t3);
  stack.Install (b2);
  stack.Install (b3);

  // IP addresses
  // Top LAN: 192.168.1.0/24
  // Bottom LAN: 192.168.2.0/24
  // WAN: 76.1.1.0/30

  // Top LAN - assign addresses just to hosts and router (interfaces used)
  Ipv4AddressHelper ipv4TopLan;
  ipv4TopLan.SetBase ("192.168.1.0", "255.255.255.0");
  // tr-ts1 interface (tr is host 1)
  Ipv4InterfaceContainer iface_trtop;
  iface_trtop = ipv4TopLan.Assign (NetDeviceContainer (nd_trts1.Get (0)));      // tr interface
  // t2-ts2 interface
  Ipv4InterfaceContainer iface_t2;
  iface_t2 = ipv4TopLan.Assign (NetDeviceContainer (nd_t2ts2.Get (0)));         // t2
  // t3-ts4 interface
  Ipv4InterfaceContainer iface_t3;
  iface_t3 = ipv4TopLan.Assign (NetDeviceContainer (nd_t3ts4.Get (0)));         // t3

  // Bottom LAN
  Ipv4AddressHelper ipv4BottomLan;
  ipv4BottomLan.SetBase ("192.168.2.0", "255.255.255.0");
  // br-bs1 interface (br is host 1)
  Ipv4InterfaceContainer iface_brbot;
  iface_brbot = ipv4BottomLan.Assign (NetDeviceContainer (nd_brbs1.Get (0)));   // br
  // b2-bs2 interface
  Ipv4InterfaceContainer iface_b2;
  iface_b2 = ipv4BottomLan.Assign (NetDeviceContainer (nd_b2bs2.Get (0)));      // b2
  // b3-bs5 interface
  Ipv4InterfaceContainer iface_b3;
  iface_b3 = ipv4BottomLan.Assign (NetDeviceContainer (nd_b3bs5.Get (0)));      // b3

  // WAN
  Ipv4AddressHelper ipv4Wan;
  ipv4Wan.SetBase ("76.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer iface_trbr;
  iface_trbr = ipv4Wan.Assign (nd_trbr);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo: t2 <-> b2 (multiple switches), t3 <-> b3 (single switch)
  uint16_t echoPort1 = 9;
  uint16_t echoPort2 = 10;

  // b2: EchoServer on b2, EchoClient on t2
  UdpEchoServerHelper echoServer1 (echoPort1);
  ApplicationContainer serverApps1 = echoServer1.Install (b2);
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (16.0));
  UdpEchoClientHelper echoClient1 (iface_b2.GetAddress (0), echoPort1);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps1 = echoClient1.Install (t2);
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (16.0));

  // b3: EchoServer on b3, EchoClient on t3
  UdpEchoServerHelper echoServer2 (echoPort2);
  ApplicationContainer serverApps2 = echoServer2.Install (b3);
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (16.0));
  UdpEchoClientHelper echoClient2 (iface_b3.GetAddress (0), echoPort2);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps2 = echoClient2.Install (t3);
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (16.0));

  // Enable PCAP at key points
  csmaTop.EnablePcap ("lan-top-ts1", nd_trts1.Get (0), true, true);    // tr interface for top LAN
  csmaTop.EnablePcap ("lan-top-t2", nd_t2ts2.Get (0), true, true);     // t2 interface
  csmaTop.EnablePcap ("lan-top-t3", nd_t3ts4.Get (0), true, true);     // t3 interface
  csmaBottom.EnablePcap ("lan-bottom-br", nd_brbs1.Get (0), true, true);   // br interface for bottom LAN
  csmaBottom.EnablePcap ("lan-bottom-b2", nd_b2bs2.Get (0), true, true);   // b2 interface
  csmaBottom.EnablePcap ("lan-bottom-b3", nd_b3bs5.Get (0), true, true);   // b3 interface
  p2p.EnablePcap ("wan-link-tr", nd_trbr.Get (0), true, true);         // tr side of WAN link
  p2p.EnablePcap ("wan-link-br", nd_trbr.Get (1), true, true);         // br side of WAN link

  Simulator::Stop (Seconds (18.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}