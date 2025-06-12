#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LanWanLanBridgingExample");

int
main (int argc, char *argv[])
{
  std::string csmaRate = "100Mbps";
  std::string csmaDelay = "6560ns";
  std::string wanRate = "10Mbps";
  std::string wanDelay = "10ms";

  CommandLine cmd;
  cmd.AddValue ("csmaRate", "CSMA link data rate (per segment, e.g., 100Mbps or 10Mbps)", csmaRate);
  cmd.AddValue ("csmaDelay", "CSMA link delay (e.g., 6560ns)", csmaDelay);
  cmd.AddValue ("wanRate", "WAN point-to-point link rate", wanRate);
  cmd.AddValue ("wanDelay", "WAN point-to-point link delay", wanDelay);
  cmd.Parse (argc, argv);

  // Topology:
  //
  //        [ t2 ]                      [ t3 ]
  //           |                         |
  //        [ sw1 ]                  [ sw3 ]
  //           |                         |
  //        [ sw2 ]---------------------/ 
  //           |
  //        [ tr ]---WAN---[ br ]
  //           |              |
  //        [ sw4 ]        [ sw5 ]
  //           |              |
  //        [ sw6 ]        [ b3 ]
  //           |
  //        [ b2 ]
  //
  // t2: UDP Echo Client           b3: UDP Echo Client
  // t3: UDP Echo Server           b2: UDP Echo Server

  // Create nodes
  Ptr<Node> t2 = CreateObject<Node> ();    // top client
  Ptr<Node> t3 = CreateObject<Node> ();    // top server
  Ptr<Node> b2 = CreateObject<Node> ();    // bottom server
  Ptr<Node> b3 = CreateObject<Node> ();    // bottom client

  Ptr<Node> tr = CreateObject<Node> ();    // top router
  Ptr<Node> br = CreateObject<Node> ();    // bottom router

  Ptr<Node> sw1 = CreateObject<Node> ();   // top lan switch 1
  Ptr<Node> sw2 = CreateObject<Node> ();   // top lan switch 2
  Ptr<Node> sw3 = CreateObject<Node> ();   // top lan switch 3
  Ptr<Node> sw4 = CreateObject<Node> ();   // bottom lan switch 4
  Ptr<Node> sw5 = CreateObject<Node> ();   // bottom lan switch 5
  Ptr<Node> sw6 = CreateObject<Node> ();   // bottom lan switch 6

  // CSMA helper for LANs
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue (csmaRate));
  csma.SetChannelAttribute ("Delay", StringValue (csmaDelay));

  // Containers for devices
  NetDeviceContainer devT2, devT3, devB2, devB3, devTrTop, devTrBot, devBrTop, devBrBot;
  NetDeviceContainer devSw1, devSw2, devSw3, devSw4, devSw5, devSw6;

  // ********
  // Top LAN
  // ********

  // t2 <-> sw1
  NetDeviceContainer ndc_t2_sw1;
  ndc_t2_sw1 = csma.Install (NodeContainer (t2, sw1));
  devT2.Add (ndc_t2_sw1.Get (0));
  devSw1.Add (ndc_t2_sw1.Get (1));

  // sw1 <-> sw2
  NetDeviceContainer ndc_sw1_sw2;
  ndc_sw1_sw2 = csma.Install (NodeContainer (sw1, sw2));
  devSw1.Add (ndc_sw1_sw2.Get (0));
  devSw2.Add (ndc_sw1_sw2.Get (1));

  // sw2 <-> tr
  NetDeviceContainer ndc_sw2_tr;
  ndc_sw2_tr = csma.Install (NodeContainer (sw2, tr));
  devSw2.Add (ndc_sw2_tr.Get (0));
  devTrTop.Add (ndc_sw2_tr.Get (1));

  // sw2 <-> sw3
  NetDeviceContainer ndc_sw2_sw3;
  ndc_sw2_sw3 = csma.Install (NodeContainer (sw2, sw3));
  devSw2.Add (ndc_sw2_sw3.Get (0));
  devSw3.Add (ndc_sw2_sw3.Get (1));

  // sw3 <-> t3
  NetDeviceContainer ndc_sw3_t3;
  ndc_sw3_t3 = csma.Install (NodeContainer (sw3, t3));
  devSw3.Add (ndc_sw3_t3.Get (0));
  devT3.Add (ndc_sw3_t3.Get (1));

  // install bridges on switches
  BridgeHelper bridge;
  Ptr<NetDevice> bridgeDev_sw1 = bridge.Install (sw1, devSw1);
  Ptr<NetDevice> bridgeDev_sw2 = bridge.Install (sw2, devSw2);
  Ptr<NetDevice> bridgeDev_sw3 = bridge.Install (sw3, devSw3);

  // ************
  // Bottom LAN
  // ************

  // b2 <-> sw6
  NetDeviceContainer ndc_b2_sw6;
  ndc_b2_sw6 = csma.Install (NodeContainer (b2, sw6));
  devB2.Add (ndc_b2_sw6.Get (0));
  devSw6.Add (ndc_b2_sw6.Get (1));

  // sw6 <-> sw4
  NetDeviceContainer ndc_sw6_sw4;
  ndc_sw6_sw4 = csma.Install (NodeContainer (sw6, sw4));
  devSw6.Add (ndc_sw6_sw4.Get (0));
  devSw4.Add (ndc_sw6_sw4.Get (1));

  // sw4 <-> br
  NetDeviceContainer ndc_sw4_br;
  ndc_sw4_br = csma.Install (NodeContainer (sw4, br));
  devSw4.Add (ndc_sw4_br.Get (0));
  devBrTop.Add (ndc_sw4_br.Get (1));

  // sw6 <-> sw5
  NetDeviceContainer ndc_sw6_sw5;
  ndc_sw6_sw5 = csma.Install (NodeContainer (sw6, sw5));
  devSw6.Add (ndc_sw6_sw5.Get (0));
  devSw5.Add (ndc_sw6_sw5.Get (1));

  // sw5 <-> b3
  NetDeviceContainer ndc_sw5_b3;
  ndc_sw5_b3 = csma.Install (NodeContainer (sw5, b3));
  devSw5.Add (ndc_sw5_b3.Get (0));
  devB3.Add (ndc_sw5_b3.Get (1));

  // install bridges on switches
  Ptr<NetDevice> bridgeDev_sw4 = bridge.Install (sw4, devSw4);
  Ptr<NetDevice> bridgeDev_sw5 = bridge.Install (sw5, devSw5);
  Ptr<NetDevice> bridgeDev_sw6 = bridge.Install (sw6, devSw6);

  // ****************
  // Routers to WAN
  // ****************
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (wanRate));
  p2p.SetChannelAttribute ("Delay", StringValue (wanDelay));
  NetDeviceContainer ndc_tr_br = p2p.Install (NodeContainer (tr, br));
  devTrBot.Add (ndc_tr_br.Get (0));
  devBrBot.Add (ndc_tr_br.Get (1));

  // *******************
  // Install Internet
  // *******************
  InternetStackHelper stack;
  stack.Install (t2);
  stack.Install (t3);
  stack.Install (b2);
  stack.Install (b3);
  stack.Install (tr);
  stack.Install (br);
  // Don't stack on switch nodes

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  // Top LAN: 192.168.1.0/24
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifc_t2, ifc_t3, ifc_tr_top;
  ifc_t2 = ipv4.Assign (devT2);
  ifc_t3 = ipv4.Assign (devT3);
  Ipv4InterfaceContainer ifc_tr_top_full = ipv4.Assign (devTrTop);

  // Bottom LAN: 192.168.2.0/24
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifc_b2, ifc_b3, ifc_br_top;
  ifc_b2 = ipv4.Assign (devB2);
  ifc_b3 = ipv4.Assign (devB3);
  Ipv4InterfaceContainer ifc_br_top_full = ipv4.Assign (devBrTop);

  // WAN: 10.1.1.0/30
  ipv4.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer ifc_tr_bot = ipv4.Assign (devTrBot);
  Ipv4InterfaceContainer ifc_br_bot = ipv4.Assign (devBrBot);

  // *******************
  // Routing setup
  // *******************
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up default routes on LAN hosts
  Ptr<Ipv4> ipv4t2 = t2->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4t3 = t3->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4b2 = b2->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4b3 = b3->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4tr = tr->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4br = br->GetObject<Ipv4> ();

  // Top LAN: Gateway is tr
  Ipv4Address trLanIfAddr = ifc_tr_top_full.GetAddress (0);

  Ipv4StaticRoutingHelper srHelper;
  Ptr<Ipv4StaticRouting> hostRoute;
  hostRoute = srHelper.GetStaticRouting (ipv4t2);
  hostRoute->SetDefaultRoute (trLanIfAddr, 1);

  hostRoute = srHelper.GetStaticRouting (ipv4t3);
  hostRoute->SetDefaultRoute (trLanIfAddr, 1);

  // Bottom LAN: Gateway is br
  Ipv4Address brLanIfAddr = ifc_br_top_full.GetAddress (0);

  hostRoute = srHelper.GetStaticRouting (ipv4b2);
  hostRoute->SetDefaultRoute (brLanIfAddr, 1);

  hostRoute = srHelper.GetStaticRouting (ipv4b3);
  hostRoute->SetDefaultRoute (brLanIfAddr, 1);

  // ***********************
  // UDP Echo Applications
  // ***********************
  double appStart = 2.0;
  double appStop = 8.0;

  // t3 (server) listens on port 9
  UdpEchoServerHelper echoServerTop (9);
  ApplicationContainer serverAppsTop = echoServerTop.Install (t3);
  serverAppsTop.Start (Seconds (appStart));
  serverAppsTop.Stop (Seconds (appStop));

  // t2 (client) sends to t3
  UdpEchoClientHelper echoClientTop (ifc_t3.GetAddress (0), 9);
  echoClientTop.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClientTop.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientTop.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientAppsTop = echoClientTop.Install (t2);
  clientAppsTop.Start (Seconds (appStart + 1));
  clientAppsTop.Stop (Seconds (appStop));

  // b2 (server) listens on port 9
  UdpEchoServerHelper echoServerBot (9);
  ApplicationContainer serverAppsBot = echoServerBot.Install (b2);
  serverAppsBot.Start (Seconds (appStart));
  serverAppsBot.Stop (Seconds (appStop));

  // b3 (client) sends to b2
  UdpEchoClientHelper echoClientBot (ifc_b2.GetAddress (0), 9);
  echoClientBot.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClientBot.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientBot.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientAppsBot = echoClientBot.Install (b3);
  clientAppsBot.Start (Seconds (appStart + 1));
  clientAppsBot.Stop (Seconds (appStop));

  // Enable PCAP tracing
  csma.EnablePcapAll ("lan-wan-lan-bridge", false);
  p2p.EnablePcapAll ("lan-wan-lan-bridge", false);

  Simulator::Stop (Seconds (appStop + 2.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}