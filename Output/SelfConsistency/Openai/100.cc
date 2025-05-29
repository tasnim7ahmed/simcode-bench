/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DualLanWanGlobalRouterExample");

int
main (int argc, char *argv[])
{
  std::string csmaRate = "100Mbps"; // Default to FastEthernet
  CommandLine cmd;
  cmd.AddValue ("csmaRate", "Data rate for CSMA (LAN) links", csmaRate);
  cmd.Parse (argc, argv);

  // Top LAN topology (ts1-ts2-ts3-ts4), endpoints t2, t3, router tr
  // Bottom LAN topology (bs1-bs2-bs3-bs4-bs5), endpoints b2, b3, router br
  // WAN: tr <-> br

  // --------- Create Nodes ----------
  // Top side
  NodeContainer topSwitches;
  topSwitches.Create (4); // ts1 ts2 ts3 ts4
  Ptr<Node> t2 = CreateObject<Node> ();
  Ptr<Node> t3 = CreateObject<Node> ();
  Ptr<Node> tr = CreateObject<Node> (); // Top router

  // Bottom side
  NodeContainer bottomSwitches;
  bottomSwitches.Create (5); // bs1 bs2 bs3 bs4 bs5
  Ptr<Node> b2 = CreateObject<Node> ();
  Ptr<Node> b3 = CreateObject<Node> ();
  Ptr<Node> br = CreateObject<Node> (); // Bottom router

  // Routers for WAN connection
  NodeContainer routers;
  routers.Add (tr);
  routers.Add (br);

  // --------- Create LAN Devices and Bridges ----------
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue (csmaRate));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // ---- Top LAN switches and bridge setup ----
  std::vector<Ptr<Node>> ts{topSwitches.Get(0), topSwitches.Get(1), topSwitches.Get(2), topSwitches.Get(3)};

  // Each switch will need one net device per connection.
  // We'll connect as: 
  // ts1 -- ts2 -- ts3 -- ts4
  // t2, tr connected to ts1.
  // t3 connected to ts4.

  // Create NetDevice containers to collect devices to bridge on each switch
  std::vector<NetDeviceContainer> topSwitchPorts (4);

  // Interconnect switches in a chain
  // ts1 <-> ts2
  NetDeviceContainer link = csma.Install (NodeContainer (ts[0], ts[1]));
  topSwitchPorts[0].Add (link.Get (0));
  topSwitchPorts[1].Add (link.Get (1));
  // ts2 <-> ts3
  link = csma.Install (NodeContainer (ts[1], ts[2]));
  topSwitchPorts[1].Add (link.Get (0));
  topSwitchPorts[2].Add (link.Get (1));
  // ts3 <-> ts4
  link = csma.Install (NodeContainer (ts[2], ts[3]));
  topSwitchPorts[2].Add (link.Get (0));
  topSwitchPorts[3].Add (link.Get (1));

  // t2 <-> ts1 
  link = csma.Install (NodeContainer (t2, ts[0]));
  Ptr<NetDevice> t2Dev = link.Get (0);
  topSwitchPorts[0].Add (link.Get (1));

  // t3 <-> ts4
  link = csma.Install (NodeContainer (t3, ts[3]));
  Ptr<NetDevice> t3Dev = link.Get (0);
  topSwitchPorts[3].Add (link.Get (1));

  // tr <-> ts1 (router to edge of LAN)
  link = csma.Install (NodeContainer (tr, ts[0]));
  Ptr<NetDevice> trLanDev = link.Get (0);
  topSwitchPorts[0].Add (link.Get (1));

  // Bridge devices on each switch
  std::vector<Ptr<NetDevice>> tsBridgeDevs(4);
  for (uint32_t i = 0; i < 4; ++i)
    {
      BridgeHelper bridge;
      tsBridgeDevs[i] = bridge.Install (ts[i], topSwitchPorts[i]);
    }
  // ---- Bottom LAN switches and bridge setup ----
  std::vector<Ptr<Node>> bs{bottomSwitches.Get(0), bottomSwitches.Get(1),
                            bottomSwitches.Get(2), bottomSwitches.Get(3),
                            bottomSwitches.Get(4)};
  // Diagram: bs1--bs2--bs3--bs4--bs5
  // b2 to bs1, br to bs1, b3 to bs5
  std::vector<NetDeviceContainer> bottomSwitchPorts (5);

  // Interconnect bs1 <-> bs2
  link = csma.Install (NodeContainer (bs[0], bs[1]));
  bottomSwitchPorts[0].Add (link.Get (0));
  bottomSwitchPorts[1].Add (link.Get (1));
  // bs2 <-> bs3
  link = csma.Install (NodeContainer (bs[1], bs[2]));
  bottomSwitchPorts[1].Add (link.Get (0));
  bottomSwitchPorts[2].Add (link.Get (1));
  // bs3 <-> bs4
  link = csma.Install (NodeContainer (bs[2], bs[3]));
  bottomSwitchPorts[2].Add (link.Get (0));
  bottomSwitchPorts[3].Add (link.Get (1));
  // bs4 <-> bs5
  link = csma.Install (NodeContainer (bs[3], bs[4]));
  bottomSwitchPorts[3].Add (link.Get (0));
  bottomSwitchPorts[4].Add (link.Get (1));

  // b2 <-> bs1
  link = csma.Install (NodeContainer (b2, bs[0]));
  Ptr<NetDevice> b2Dev = link.Get (0);
  bottomSwitchPorts[0].Add (link.Get (1));

  // br <-> bs1 (router to edge)
  link = csma.Install (NodeContainer (br, bs[0]));
  Ptr<NetDevice> brLanDev = link.Get (0);
  bottomSwitchPorts[0].Add (link.Get (1));

  // b3 <-> bs5
  link = csma.Install (NodeContainer (b3, bs[4]));
  Ptr<NetDevice> b3Dev = link.Get (0);
  bottomSwitchPorts[4].Add (link.Get (1));

  // Bridge devices on each switch
  std::vector<Ptr<NetDevice>> bsBridgeDevs(5);
  for (uint32_t i = 0; i < 5; ++i)
    {
      BridgeHelper bridge;
      bsBridgeDevs[i] = bridge.Install (bs[i], bottomSwitchPorts[i]);
    }

  // --------- Set up WAN: tr <-> br ----------
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));
  NetDeviceContainer wanDevices = p2p.Install (NodeContainer (tr, br));
  Ptr<NetDevice> trWanDev = wanDevices.Get (0);
  Ptr<NetDevice> brWanDev = wanDevices.Get (1);

  // --------- Install Internet stack ----------
  InternetStackHelper stack;
  // All hosts, routers will run IP stack
  NodeContainer hostsAndRouters;
  hostsAndRouters.Add (t2);
  hostsAndRouters.Add (t3);
  hostsAndRouters.Add (tr);
  hostsAndRouters.Add (b2);
  hostsAndRouters.Add (b3);
  hostsAndRouters.Add (br);
  stack.Install (hostsAndRouters);

  // --------- Assign IP addresses ----------
  Ipv4AddressHelper address;

  // Top LAN: 192.168.1.0/24
  address.SetBase ("192.168.1.0", "255.255.255.0");
  std::vector<Ipv4InterfaceContainer> topLanIfs(3);
  // Order: t2, t3, tr
  topLanIfs[0] = address.Assign (NetDeviceContainer (t2Dev));
  address.NewNetwork ();
  topLanIfs[1] = address.Assign (NetDeviceContainer (t3Dev));
  address.NewNetwork ();
  topLanIfs[2] = address.Assign (NetDeviceContainer (trLanDev));
  // Because bridge interfaces have no IP, only end hosts (and router) need IPs

  // Bottom LAN: 192.168.2.0/24
  address.SetBase ("192.168.2.0", "255.255.255.0");
  std::vector<Ipv4InterfaceContainer> bottomLanIfs(3);
  // Order: b2, b3, br
  bottomLanIfs[0] = address.Assign (NetDeviceContainer (b2Dev));
  address.NewNetwork ();
  bottomLanIfs[1] = address.Assign (NetDeviceContainer (b3Dev));
  address.NewNetwork ();
  bottomLanIfs[2] = address.Assign (NetDeviceContainer (brLanDev));

  // WAN: 76.1.1.0/30
  address.SetBase ("76.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer wanIfs = address.Assign (wanDevices); // tr, br

  // --------- Set up routing ---------
  // Using Global Routing (which will examine switch topology via global-router-interface)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // --------- UDP Echo Applications ---------
  // Test 1: t2 (client) <-> b2 (server)
  uint16_t echoPort1 = 9;
  UdpEchoServerHelper echoServer1 (echoPort1);
  ApplicationContainer serverApps1 = echoServer1.Install (b2);
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (10.0));
  UdpEchoClientHelper echoClient1 (bottomLanIfs[0].GetAddress (0), echoPort1); // b2's IP
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps1 = echoClient1.Install (t2);
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (10.0));

  // Test 2: t3 (client) <-> b3 (server)
  uint16_t echoPort2 = 10;
  UdpEchoServerHelper echoServer2 (echoPort2);
  ApplicationContainer serverApps2 = echoServer2.Install (b3);
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));
  UdpEchoClientHelper echoClient2 (bottomLanIfs[1].GetAddress (0), echoPort2); // b3's IP
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps2 = echoClient2.Install (t3);
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  // --------- Enable PCAP tracing at key locations ---------
  // WAN link
  p2p.EnablePcap ("wan", wanDevices, true);
  // LAN router uplinks
  csma.EnablePcap ("tr-lan", trLanDev, true);
  csma.EnablePcap ("br-lan", brLanDev, true);
  // Switches: capture at ts2, ts4 (top) and bs2, bs5 (bottom)
  csma.EnablePcap ("ts2-bridge", tsBridgeDevs[1], true);
  csma.EnablePcap ("ts4-bridge", tsBridgeDevs[3], true);
  csma.EnablePcap ("bs2-bridge", bsBridgeDevs[1], true);
  csma.EnablePcap ("bs5-bridge", bsBridgeDevs[4], true);

  // --------- Run Simulation ----------
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}