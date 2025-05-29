#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LanWanLanGlobalRouterExample");

int
main (int argc, char *argv[])
{
  std::string csmaDataRate = "100Mbps";
  CommandLine cmd;
  cmd.AddValue ("csmaDataRate", "CSMA link speed (e.g., 100Mbps or 10Mbps)", csmaDataRate);
  cmd.Parse (argc, argv);

  // Top LAN configuration
  NodeContainer t2, t3; // endpoints
  t2.Create (1);
  t3.Create (1);

  NodeContainer ts1, ts2, ts3, ts4; // switches
  ts1.Create (1);
  ts2.Create (1);
  ts3.Create (1);
  ts4.Create (1);

  NodeContainer tr; // router
  tr.Create (1);

  // Bottom LAN configuration
  NodeContainer b2, b3;
  b2.Create (1);
  b3.Create (1);

  NodeContainer bs1, bs2, bs3, bs4, bs5;
  bs1.Create (1);
  bs2.Create (1);
  bs3.Create (1);
  bs4.Create (1);
  bs5.Create (1);

  NodeContainer br;
  br.Create (1);

  // WAN
  NodeContainer wanRouters;
  wanRouters.Add (tr.Get (0));
  wanRouters.Add (br.Get (0));

  // CSMA helpers (for LANs)
  CsmaHelper csmaTop, csmaBottom;
  csmaTop.SetChannelAttribute ("DataRate", StringValue (csmaDataRate));
  csmaTop.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csmaBottom.SetChannelAttribute ("DataRate", StringValue (csmaDataRate));
  csmaBottom.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Point-to-point helper (for WAN)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));

  // ---------------- L2 TOPOLOGY: TOP LAN -----------------
  NetDeviceContainer t2Dev, t3Dev;
  NetDeviceContainer ts1Ports, ts2Ports, ts3Ports, ts4Ports, trPortsLAN;

  // t2 <---> ts1 <---> ts2 <---> ts3 <---> ts4 <---> tr

  // t2 <-> ts1
  NetDeviceContainer linkT2Ts1 = csmaTop.Install (NodeContainer (t2, ts1));
  t2Dev.Add (linkT2Ts1.Get (0));
  ts1Ports.Add (linkT2Ts1.Get (1));

  // ts1 <-> ts2
  NetDeviceContainer linkTs1Ts2 = csmaTop.Install (NodeContainer (ts1, ts2));
  ts1Ports.Add (linkTs1Ts2.Get (0));
  ts2Ports.Add (linkTs1Ts2.Get (1));

  // ts2 <-> ts3
  NetDeviceContainer linkTs2Ts3 = csmaTop.Install (NodeContainer (ts2, ts3));
  ts2Ports.Add (linkTs2Ts3.Get (0));
  ts3Ports.Add (linkTs2Ts3.Get (1));

  // ts3 <-> ts4
  NetDeviceContainer linkTs3Ts4 = csmaTop.Install (NodeContainer (ts3, ts4));
  ts3Ports.Add (linkTs3Ts4.Get (0));
  ts4Ports.Add (linkTs3Ts4.Get (1));

  // ts4 <-> tr
  NetDeviceContainer linkTs4Tr = csmaTop.Install (NodeContainer (ts4, tr));
  ts4Ports.Add (linkTs4Tr.Get (0));
  trPortsLAN.Add (linkTs4Tr.Get (1));

  // t3 <-> ts2 (single-switch path)
  NetDeviceContainer linkT3Ts2 = csmaTop.Install (NodeContainer (t3, ts2));
  t3Dev.Add (linkT3Ts2.Get (0));
  ts2Ports.Add (linkT3Ts2.Get (1));

  // Install bridges on each switch of top LAN
  BridgeHelper bridge;
  NetDeviceContainer ts1Bridge = bridge.Install (ts1, ts1Ports);
  NetDeviceContainer ts2Bridge = bridge.Install (ts2, ts2Ports);
  NetDeviceContainer ts3Bridge = bridge.Install (ts3, ts3Ports);
  NetDeviceContainer ts4Bridge = bridge.Install (ts4, ts4Ports);

  // ---------------- L2 TOPOLOGY: BOTTOM LAN ----------------
  NetDeviceContainer b2Dev, b3Dev;
  NetDeviceContainer bs1Ports, bs2Ports, bs3Ports, bs4Ports, bs5Ports, brPortsLAN;

  // b2 <---> bs1 <---> bs2 <---> bs3 <---> bs4 <---> bs5 <---> br

  // b2 <-> bs1
  NetDeviceContainer linkB2Bs1 = csmaBottom.Install (NodeContainer (b2, bs1));
  b2Dev.Add (linkB2Bs1.Get (0));
  bs1Ports.Add (linkB2Bs1.Get (1));

  // bs1 <-> bs2
  NetDeviceContainer linkBs1Bs2 = csmaBottom.Install (NodeContainer (bs1, bs2));
  bs1Ports.Add (linkBs1Bs2.Get (0));
  bs2Ports.Add (linkBs1Bs2.Get (1));

  // bs2 <-> bs3
  NetDeviceContainer linkBs2Bs3 = csmaBottom.Install (NodeContainer (bs2, bs3));
  bs2Ports.Add (linkBs2Bs3.Get (0));
  bs3Ports.Add (linkBs2Bs3.Get (1));

  // bs3 <-> bs4
  NetDeviceContainer linkBs3Bs4 = csmaBottom.Install (NodeContainer (bs3, bs4));
  bs3Ports.Add (linkBs3Bs4.Get (0));
  bs4Ports.Add (linkBs3Bs4.Get (1));

  // bs4 <-> bs5
  NetDeviceContainer linkBs4Bs5 = csmaBottom.Install (NodeContainer (bs4, bs5));
  bs4Ports.Add (linkBs4Bs5.Get (0));
  bs5Ports.Add (linkBs4Bs5.Get (1));

  // bs5 <-> br
  NetDeviceContainer linkBs5Br = csmaBottom.Install (NodeContainer (bs5, br));
  bs5Ports.Add (linkBs5Br.Get (0));
  brPortsLAN.Add (linkBs5Br.Get (1));

  // b3 <-> bs3 (single-switch path)
  NetDeviceContainer linkB3Bs3 = csmaBottom.Install (NodeContainer (b3, bs3));
  b3Dev.Add (linkB3Bs3.Get (0));
  bs3Ports.Add (linkB3Bs3.Get (1));

  NetDeviceContainer bs1Bridge = bridge.Install (bs1, bs1Ports);
  NetDeviceContainer bs2Bridge = bridge.Install (bs2, bs2Ports);
  NetDeviceContainer bs3Bridge = bridge.Install (bs3, bs3Ports);
  NetDeviceContainer bs4Bridge = bridge.Install (bs4, bs4Ports);
  NetDeviceContainer bs5Bridge = bridge.Install (bs5, bs5Ports);

  // ------------------- WAN LINK -----------------------
  NetDeviceContainer trPortWAN, brPortWAN;
  NetDeviceContainer wanDevs = p2p.Install (NodeContainer (tr, br));
  trPortWAN.Add (wanDevs.Get (0)); // tr's WAN port
  brPortWAN.Add (wanDevs.Get (1)); // br's WAN port

  // ------------ Combine all nodes for InternetStack ------
  NodeContainer allHosts;
  allHosts.Add (t2); allHosts.Add (t3);
  allHosts.Add (b2); allHosts.Add (b3);

  NodeContainer allRouters;
  allRouters.Add (tr); allRouters.Add (br);

  InternetStackHelper stack;
  stack.Install (t2);
  stack.Install (t3);
  stack.Install (b2);
  stack.Install (b3);
  stack.Install (tr);
  stack.Install (br);

  // ------------ Assign IP addresses ----------------------
  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer interfaces;

  // Top LAN: 192.168.1.0/24
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer t2If = ipv4.Assign (t2Dev);
  Ipv4InterfaceContainer t3If = ipv4.Assign (t3Dev);
  Ipv4InterfaceContainer trIfLAN = ipv4.Assign (trPortsLAN);

  // Bottom LAN: 192.168.2.0/24
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer b2If = ipv4.Assign (b2Dev);
  Ipv4InterfaceContainer b3If = ipv4.Assign (b3Dev);
  Ipv4InterfaceContainer brIfLAN = ipv4.Assign (brPortsLAN);

  // WAN: 76.1.1.0/24
  ipv4.SetBase ("76.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer trIfWAN = ipv4.Assign (trPortWAN);
  Ipv4InterfaceContainer brIfWAN = ipv4.Assign (brPortWAN);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // ------------ Applications: UDP Echo ---------------
  uint16_t echoPort = 9;

  // On b2, set up UDP echo server
  UdpEchoServerHelper echoServerB2 (echoPort);
  ApplicationContainer serverAppsB2 = echoServerB2.Install (b2.Get (0));
  serverAppsB2.Start (Seconds (1.0));
  serverAppsB2.Stop (Seconds (10.0));

  // On b3, set up UDP echo server
  UdpEchoServerHelper echoServerB3 (echoPort);
  ApplicationContainer serverAppsB3 = echoServerB3.Install (b3.Get (0));
  serverAppsB3.Start (Seconds (1.0));
  serverAppsB3.Stop (Seconds (10.0));

  // On t2, echo client to b2
  UdpEchoClientHelper echoClientT2 (b2If.GetAddress (0), echoPort);
  echoClientT2.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClientT2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientT2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientAppsT2 = echoClientT2.Install (t2.Get (0));
  clientAppsT2.Start (Seconds (2.0));
  clientAppsT2.Stop (Seconds (10.0));

  // On t3, echo client to b3
  UdpEchoClientHelper echoClientT3 (b3If.GetAddress (0), echoPort);
  echoClientT3.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClientT3.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientT3.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientAppsT3 = echoClientT3.Install (t3.Get (0));
  clientAppsT3.Start (Seconds (2.0));
  clientAppsT3.Stop (Seconds (10.0));

  // Enable PCAP tracing at key locations
  csmaTop.EnablePcap ("lan_top_t2", t2Dev.Get (0), true);
  csmaTop.EnablePcap ("lan_top_t3", t3Dev.Get (0), true);
  csmaTop.EnablePcap ("lan_top_tr", trPortsLAN.Get (0), true);

  csmaBottom.EnablePcap ("lan_bottom_b2", b2Dev.Get (0), true);
  csmaBottom.EnablePcap ("lan_bottom_b3", b3Dev.Get (0), true);
  csmaBottom.EnablePcap ("lan_bottom_br", brPortsLAN.Get (0), true);

  p2p.EnablePcap ("wan_tr", trPortWAN.Get (0), true);
  p2p.EnablePcap ("wan_br", brPortWAN.Get (0), true);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}