/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Mixed wired/wireless topology:
 *  - Multiple backbone routers interconnected via ad hoc WiFi using OLSR
 *  - Each backbone router:
 *      - Has a p2p or csma LAN attached (wired nodes)
 *      - Has an 802.11 AP (infrastructure BSS)
 *      - Each AP has associated STAs (wireless nodes)
 *  - UDP flow from a LAN node (sender) to a wireless STA (receiver) in last backbone
 *  - Command line parameters: nBackbone, nLanNodes, nStaNodes, tracing
 *  - Mobility for all wireless and backbone nodes, with course change logging
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWiredWirelessExample");

void
CourseChangeCallback (std::string context, Ptr<const MobilityModel> model)
{
  Vector pos = model->GetPosition ();
  NS_LOG_INFO (Simulator::Now ().GetSeconds ()
               << "s: Node " << context
               << " moved to: " << pos);
}

int
main (int argc, char *argv[])
{
  // Simulation parameters
  uint32_t nBackbone = 3;
  uint32_t nLanNodes = 3;
  uint32_t nStaNodes = 3;
  double simulationTime = 20.0; // seconds
  bool enableTracing = false;

  CommandLine cmd;
  cmd.AddValue ("nBackbone", "Number of backbone routers (>=2)", nBackbone);
  cmd.AddValue ("nLanNodes", "Number of wired LAN nodes per router", nLanNodes);
  cmd.AddValue ("nStaNodes", "Number of WiFi STA (infra) per router", nStaNodes);
  cmd.AddValue ("tracing", "Enable pcap and trace output", enableTracing);
  cmd.Parse (argc, argv);

  if (nBackbone < 2)
    {
      NS_ABORT_MSG ("At least two backbone routers required (see nBackbone)");
    }
  NS_LOG_UNCOND ("Backbone: " << nBackbone << ", LAN/node: " << nLanNodes << ", WiFi/STA: " << nStaNodes);

  // Containers for backbone routers
  NodeContainer backboneRouters;
  backboneRouters.Create (nBackbone);

  // For simpler referencing, make vectors of node containers
  std::vector<NodeContainer> lanNodes (nBackbone);
  std::vector<NodeContainer> apNodes (nBackbone);
  std::vector<NodeContainer> staNodes (nBackbone);

  // Each backbone router gets LAN and AP
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      // Wired LAN nodes (not including router)
      lanNodes[i].Create (nLanNodes);

      // WiFi AP is just the backbone router
      apNodes[i].Add (backboneRouters.Get (i));

      // Infrastructure STA nodes
      staNodes[i].Create (nStaNodes);
    }

  // 1. Setup Ad Hoc WiFi (backbone interconnection)
  YansWifiChannelHelper adhocChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper adhocPhy = YansWifiPhyHelper::Default ();
  adhocPhy.SetChannel (adhocChannel.Create ());

  WifiHelper adhocWifi;
  adhocWifi.SetStandard (WIFI_STANDARD_80211b);
  WifiMacHelper adhocMac;
  adhocMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer backboneWifiDevices =
    adhocWifi.Install (adhocPhy, adhocMac, backboneRouters);

  // 2. Setup CSMA LANs
  std::vector<NetDeviceContainer> lanDevices (nBackbone);
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      NodeContainer lanSegment;
      // Each segment: router + LAN nodes
      lanSegment.Add (backboneRouters.Get (i));
      lanSegment.Add (lanNodes[i]);
      lanDevices[i] = csma.Install (lanSegment);
    }

  // 3. Setup Infrastructure WiFi (AP+STAs)
  std::vector<NetDeviceContainer> staDevices (nBackbone);
  std::vector<NetDeviceContainer> apDevices (nBackbone);
  YansWifiChannelHelper infraChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper infraPhy = YansWifiPhyHelper::Default ();
  infraPhy.SetChannel (infraChannel.Create ());

  WifiHelper infraWifi;
  infraWifi.SetStandard (WIFI_STANDARD_80211b);

  Ssid ssid;
  WifiMacHelper staMac, apMac;
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      std::ostringstream oss;
      oss << "infra-ssid-" << i;

      ssid = Ssid (oss.str ());
      // STAs
      staMac.SetType ("ns3::StaWifiMac",
                      "Ssid", SsidValue (ssid),
                      "ActiveProbing", BooleanValue (false));
      staDevices[i] = infraWifi.Install (infraPhy, staMac, staNodes[i]);
      // AP
      apMac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));
      apDevices[i] = infraWifi.Install (infraPhy, apMac, apNodes[i]);
    }

  // 4. Install Internet Stack
  OlsrHelper olsr;
  InternetStackHelper stack;
  stack.SetRoutingHelper (olsr); // Use OLSR on all nodes for optimal routes

  // All nodes: backbone, LANs, STAs
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      stack.Install (backboneRouters.Get (i));
      stack.Install (lanNodes[i]);
      stack.Install (staNodes[i]);
    }

  // 5. Addressing
  Ipv4AddressHelper address;

  // Assign subnets: backbone (Ad Hoc)
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneIfs = address.Assign (backboneWifiDevices);

  // Assign subnets: LANs
  std::vector<Ipv4InterfaceContainer> lanIfs (nBackbone);
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.2." << i << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      lanIfs[i] = address.Assign (lanDevices[i]);
    }

  // Assign subnets: WiFi Infra
  std::vector<Ipv4InterfaceContainer> staIfs (nBackbone);
  std::vector<Ipv4InterfaceContainer> apIfs (nBackbone);
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.3." << i << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      staIfs[i] = address.Assign (staDevices[i]);
      apIfs[i] = address.Assign (apDevices[i]);
    }

  // 6. Mobility: grid for backbone routers, random for others
  MobilityHelper backboneMob;
  backboneMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  backboneMob.Install (backboneRouters);
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      Ptr<Node> bnode = backboneRouters.Get (i);
      Ptr<MobilityModel> mm = bnode->GetObject<MobilityModel> ();
      mm->SetPosition (Vector (30 * i, 0, 0));
    }

  // Wireless APs: fixed above each backbone router
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      MobilityHelper apMob;
      apMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      apMob.Install (apNodes[i]);
      Ptr<MobilityModel> mm = apNodes[i].Get (0)->GetObject<MobilityModel> ();
      mm->SetPosition (Vector (30 * i, 0, 1.5));
    }

  // LAN nodes: fixed near router
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      MobilityHelper lanMob;
      lanMob.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (30 * i - 10),
                                   "MinY", DoubleValue (5.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "GridWidth", UintegerValue (nLanNodes),
                                   "LayoutType", StringValue ("RowFirst"));
      lanMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      lanMob.Install (lanNodes[i]);
    }

  // STA nodes: random walk near AP
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      MobilityHelper staMob;
      staMob.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                   "X", DoubleValue (30 * i),
                                   "Y", DoubleValue (0.0),
                                   "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=8]"));
      staMob.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", TimeValue (Seconds (2.0)),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                               "Bounds", RectangleValue (Rectangle (30 * i - 10, 30 * i + 10, -10, 10)));
      staMob.Install (staNodes[i]);
    }

  // 7. Tracing and Mobility Callbacks
  if (enableTracing)
    {
      AsciiTraceHelper ascii;
      // All backbone WiFi
      adhocPhy.EnablePcapAll ("mixed-topo-backbone");
      // LANs
      for (uint32_t i = 0; i < nBackbone; ++i)
        {
          csma.EnablePcap ("mixed-topo-lan", lanDevices[i], true);
          infraPhy.EnablePcap ("mixed-topo-wifi", apDevices[i].Get (0), true);
        }
    }

  // Mobility change: subscribe for all mobile nodes
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      for (uint32_t j = 0; j < nStaNodes; ++j)
        {
          Ptr<Node> node = staNodes[i].Get (j);
          Config::Connect ("/NodeList/" + std::to_string (node->GetId ()) + "/$ns3::MobilityModel/CourseChange",
                           MakeCallback (&CourseChangeCallback));
        }
    }

  // 8. UDP Flow: LAN node 0 (last backbone's LAN) -> infrastructure STA 0 in last backbone
  Ptr<Node> srcNode = lanNodes[0].Get (0);
  Ptr<Node> dstNode = staNodes[nBackbone - 1].Get (0);

  uint16_t port = 49153;
  // Destination Address: infra STA in last backbone
  Ipv4Address dstAddr = staIfs[nBackbone-1].GetAddress (0);

  // UDP Sink on receiver STA
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (dstNode);
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (simulationTime - 1.0));

  // OnOffApp UDP source on wired LAN node 0
  OnOffHelper clientHelper ("ns3::UdpSocketFactory",
                            InetSocketAddress (dstAddr, port));
  clientHelper.SetAttribute ("DataRate", StringValue ("2Mbps"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime - 1.0)));
  ApplicationContainer clientApp = clientHelper.Install (srcNode);

  // 9. FlowMonitor (optional but recommended)
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 10. Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // FlowMonitor summary
  monitor->CheckForLostPackets ();
  if (enableTracing)
    {
      monitor->SerializeToXmlFile ("mixed-topo-flowmon.xml", true, true);
    }

  Simulator::Destroy ();
  return 0;
}