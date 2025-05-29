#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("L2LanMultiSwitchWithWan");

int 
main (int argc, char *argv[])
{
  std::string lanSpeed = "100Mbps";
  std::string lanDelay = "6560ns";
  std::string p2pRate = "10Mbps";
  std::string p2pDelay = "50ms";

  CommandLine cmd;
  cmd.AddValue ("lanSpeed", "Data rate for the CSMA LAN links (100Mbps or 10Mbps)", lanSpeed);
  cmd.AddValue ("lanDelay", "Propagation delay for the LAN links", lanDelay);
  cmd.AddValue ("p2pRate", "PPP WAN rate", p2pRate);
  cmd.AddValue ("p2pDelay", "Point-to-point delay", p2pDelay);
  cmd.Parse (argc, argv);

  // Set up Internet stack
  InternetStackHelper internet;
  internet.SetIpv6StackInstall (false);

  // --- Top LAN ---
  // Nodes: t2 (client), t3 (server), tr (router)
  NodeContainer topNodes;
  topNodes.Create (3); // 0: t2, 1: t3, 2: tr

  Ptr<Node> t2 = topNodes.Get(0);
  Ptr<Node> t3 = topNodes.Get(1);
  Ptr<Node> tr = topNodes.Get(2);

  // Create switches for top LAN
  NodeContainer topSwitch1, topSwitch2; // NodeContainers for switches
  topSwitch1.Create (1);
  topSwitch2.Create (1);

  // Putting all bridge devices in a single container makes assignment easier
  NodeContainer topAllNodes;
  topAllNodes.Add (t2);
  topAllNodes.Add (t3);
  topAllNodes.Add (tr);
  topAllNodes.Add (topSwitch1);
  topAllNodes.Add (topSwitch2);

  // --- Bottom LAN ---
  // Nodes: b2 (server), b3 (client), br (router)
  NodeContainer bottomNodes;
  bottomNodes.Create (3); // 0: b2, 1: b3, 2: br

  Ptr<Node> b2 = bottomNodes.Get(0);
  Ptr<Node> b3 = bottomNodes.Get(1);
  Ptr<Node> br = bottomNodes.Get(2);

  // Create switches for bottom LAN
  NodeContainer bottomSwitch1, bottomSwitch2;
  bottomSwitch1.Create (1);
  bottomSwitch2.Create (1);

  NodeContainer bottomAllNodes;
  bottomAllNodes.Add (b2);
  bottomAllNodes.Add (b3);
  bottomAllNodes.Add (br);
  bottomAllNodes.Add (bottomSwitch1);
  bottomAllNodes.Add (bottomSwitch2);

  // --- Top LAN Initiation ---
  CsmaHelper csmaTop;
  csmaTop.SetChannelAttribute ("DataRate", StringValue (lanSpeed));
  csmaTop.SetChannelAttribute ("Delay", StringValue (lanDelay));

  // Connections for Top LAN:
  // t2 <---> sw1 <---> sw2 <---> tr
  // t3 <---> sw1
  // sw1 <--> sw2

  // sw1-ports: t2, t3, sw2
  // sw2-ports: sw1, tr

  // For every CSMA link, endpoints must be paired.

  // Connect t2 <-> sw1
  NetDeviceContainer ndc_t2_sw1 = csmaTop.Install (NodeContainer(t2, topSwitch1.Get(0)));
  // Connect t3 <-> sw1
  NetDeviceContainer ndc_t3_sw1 = csmaTop.Install (NodeContainer(t3, topSwitch1.Get(0)));
  // Connect sw1 <-> sw2
  NetDeviceContainer ndc_sw1_sw2 = csmaTop.Install (NodeContainer(topSwitch1.Get(0), topSwitch2.Get(0)));
  // Connect sw2 <-> tr
  NetDeviceContainer ndc_sw2_tr = csmaTop.Install (NodeContainer(topSwitch2.Get(0), tr));

  // Bridge setup for switches
  BridgeHelper bridge;

  // sw1 gets: t2, t3, sw2
  NetDeviceContainer sw1Ports;
  sw1Ports.Add (ndc_t2_sw1.Get (1));
  sw1Ports.Add (ndc_t3_sw1.Get (1));
  sw1Ports.Add (ndc_sw1_sw2.Get (0));
  bridge.Install (topSwitch1.Get(0), sw1Ports);

  // sw2 gets: sw1, tr
  NetDeviceContainer sw2Ports;
  sw2Ports.Add (ndc_sw1_sw2.Get (1));
  sw2Ports.Add (ndc_sw2_tr.Get (0));
  bridge.Install (topSwitch2.Get(0), sw2Ports);

  // Router tr gets ndc_sw2_tr.Get(1)

  // --- Bottom LAN Initiation ---
  CsmaHelper csmaBottom;
  csmaBottom.SetChannelAttribute ("DataRate", StringValue (lanSpeed));
  csmaBottom.SetChannelAttribute ("Delay", StringValue (lanDelay));

  // Connections for Bottom LAN:
  // b2 <---> sw1 <---> sw2 <---> br
  // b3 <---> sw1
  // sw1 <--> sw2

  // b2 <-> sw1
  NetDeviceContainer ndc_b2_sw1 = csmaBottom.Install (NodeContainer(b2, bottomSwitch1.Get(0)));
  // b3 <-> sw1
  NetDeviceContainer ndc_b3_sw1 = csmaBottom.Install (NodeContainer(b3, bottomSwitch1.Get(0)));
  // sw1 <-> sw2
  NetDeviceContainer ndc_bsw1_bsw2 = csmaBottom.Install (NodeContainer(bottomSwitch1.Get(0), bottomSwitch2.Get(0)));
  // sw2 <-> br
  NetDeviceContainer ndc_bsw2_br = csmaBottom.Install (NodeContainer(bottomSwitch2.Get(0), br));

  // Bridges
  // sw1 gets: b2, b3, sw2
  NetDeviceContainer bsw1Ports;
  bsw1Ports.Add (ndc_b2_sw1.Get(1));
  bsw1Ports.Add (ndc_b3_sw1.Get(1));
  bsw1Ports.Add (ndc_bsw1_bsw2.Get(0));
  bridge.Install (bottomSwitch1.Get(0), bsw1Ports);

  // sw2 gets: sw1, br
  NetDeviceContainer bsw2Ports;
  bsw2Ports.Add (ndc_bsw1_bsw2.Get(1));
  bsw2Ports.Add (ndc_bsw2_br.Get(0));
  bridge.Install (bottomSwitch2.Get(0), bsw2Ports);

  // Router br gets ndc_bsw2_br.Get(1)

  // --- WAN (p2p) connection between tr and br ---
  NodeContainer routers (tr, br);
  PointToPointHelper p2pWan;
  p2pWan.SetDeviceAttribute ("DataRate", StringValue (p2pRate));
  p2pWan.SetChannelAttribute ("Delay", StringValue (p2pDelay));
  NetDeviceContainer wanDevices = p2pWan.Install (routers);

  // Stack install
  for (uint32_t i = 0; i < topNodes.GetN (); ++i)
    {
      internet.Install (topNodes.Get (i));
    }
  internet.Install (topSwitch1);
  internet.Install (topSwitch2);

  for (uint32_t i = 0; i < bottomNodes.GetN (); ++i)
    {
      internet.Install (bottomNodes.Get (i));
    }
  internet.Install (bottomSwitch1);
  internet.Install (bottomSwitch2);

  // --- Addressing ---

  // Top LAN subnet
  Ipv4AddressHelper ipv4Top;
  ipv4Top.SetBase ("192.168.1.0", "255.255.255.0");

  // List of all devices on the top LAN segment (t2, t3, tr's LAN-facing)
  NetDeviceContainer topLanDevices;
  topLanDevices.Add (ndc_t2_sw1.Get(0)); // t2
  topLanDevices.Add (ndc_t3_sw1.Get(0)); // t3
  topLanDevices.Add (ndc_sw2_tr.Get(1)); // tr

  Ipv4InterfaceContainer topLanIfs = ipv4Top.Assign (topLanDevices);

  // tr's LAN interface is [2] (t2 is [0], t3 is [1])
  Ipv4Address trLanAddr = topLanIfs.GetAddress (2);

  // Bottom LAN subnet
  Ipv4AddressHelper ipv4Bottom;
  ipv4Bottom.SetBase ("192.168.2.0", "255.255.255.0");

  NetDeviceContainer bottomLanDevices;
  bottomLanDevices.Add (ndc_b2_sw1.Get(0));    // b2
  bottomLanDevices.Add (ndc_b3_sw1.Get(0));    // b3
  bottomLanDevices.Add (ndc_bsw2_br.Get(1)); // br

  Ipv4InterfaceContainer bottomLanIfs = ipv4Bottom.Assign (bottomLanDevices);

  // br's LAN interface is [2] (b2 is [0], b3 is [1])
  Ipv4Address brLanAddr = bottomLanIfs.GetAddress (2);

  // WAN subnet
  Ipv4AddressHelper ipv4Wan;
  ipv4Wan.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer wanIfs = ipv4Wan.Assign (wanDevices);
  // tr: wanIfs.GetAddress(0)
  // br: wanIfs.GetAddress(1)

  // Set up static routing
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  // t2 and t3: default route via 192.168.1.254 (tr)
  Ptr<Ipv4StaticRouting> t2Static = ipv4RoutingHelper.GetStaticRouting (t2->GetObject<Ipv4> ());
  t2Static->SetDefaultRoute (trLanAddr, 1);

  Ptr<Ipv4StaticRouting> t3Static = ipv4RoutingHelper.GetStaticRouting (t3->GetObject<Ipv4> ());
  t3Static->SetDefaultRoute (trLanAddr, 1);

  // b2 and b3: default route via 192.168.2.254 (br)
  Ptr<Ipv4StaticRouting> b2Static = ipv4RoutingHelper.GetStaticRouting (b2->GetObject<Ipv4> ());
  b2Static->SetDefaultRoute (brLanAddr, 1);

  Ptr<Ipv4StaticRouting> b3Static = ipv4RoutingHelper.GetStaticRouting (b3->GetObject<Ipv4> ());
  b3Static->SetDefaultRoute (brLanAddr, 1);

  // tr routes
  Ptr<Ipv4StaticRouting> trStatic = ipv4RoutingHelper.GetStaticRouting (tr->GetObject<Ipv4> ());
  trStatic->AddNetworkRouteTo (Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"),
                               wanIfs.GetAddress(1), 2); // via p2p to br

  // br routes
  Ptr<Ipv4StaticRouting> brStatic = ipv4RoutingHelper.GetStaticRouting (br->GetObject<Ipv4> ());
  brStatic->AddNetworkRouteTo (Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"),
                               wanIfs.GetAddress(0), 2); // via p2p to tr

  // --- UDP Echo Applications ---

  // Top t2 (client) -> Bottom b2 (server)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServerBottom (echoPort);
  ApplicationContainer serverAppsB2 = echoServerBottom.Install (b2);
  serverAppsB2.Start (Seconds (1.0));
  serverAppsB2.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClientB2 (bottomLanIfs.GetAddress(0), echoPort); // b2's ip
  echoClientB2.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClientB2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientB2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientAppsT2 = echoClientB2.Install (t2);
  clientAppsT2.Start (Seconds (2.0));
  clientAppsT2.Stop (Seconds (10.0));

  // Bottom b3 (client) -> Top t3 (server)
  UdpEchoServerHelper echoServerTop (echoPort + 1);
  ApplicationContainer serverAppsT3 = echoServerTop.Install (t3);
  serverAppsT3.Start (Seconds (1.0));
  serverAppsT3.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClientT3 (topLanIfs.GetAddress(1), echoPort + 1); // t3's ip
  echoClientT3.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClientT3.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientT3.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientAppsB3 = echoClientT3.Install (b3);
  clientAppsB3.Start (Seconds (2.0));
  clientAppsB3.Stop (Seconds (10.0));

  // Enable global routing on routers for completeness (not strictly required with static routing)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}