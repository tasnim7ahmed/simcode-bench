#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWiredWirelessSimulation");

void CourseChangeCallback (std::string path, Ptr<const MobilityModel> model)
{
  Vector pos = model->GetPosition ();
  std::cout << Simulator::Now ().GetSeconds () << "s " << path
            << " position: x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z << std::endl;
}

int main (int argc, char *argv[])
{
  uint32_t nBackboneRouters = 3;
  uint32_t nLanNodes = 2;
  uint32_t nWifiStaNodes = 2;
  bool enableTracing = true;
  double simulationTime = 20.0;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nBackboneRouters", "Number of backbone routers (>=2)", nBackboneRouters);
  cmd.AddValue ("nLanNodes", "Number of LAN nodes per backbone", nLanNodes);
  cmd.AddValue ("nWifiStaNodes", "Number of wireless STA nodes per backbone", nWifiStaNodes);
  cmd.AddValue ("enableTracing", "Enable pcap and animation tracing", enableTracing);
  cmd.AddValue ("simulationTime", "Duration of simulation (seconds)", simulationTime);
  cmd.Parse (argc, argv);

  NS_ASSERT_MSG (nBackboneRouters >= 2, "At least two backbone routers required.");

  // Containers
  NodeContainer backboneRouters;
  backboneRouters.Create (nBackboneRouters);

  std::vector<NodeContainer> lanNodes (nBackboneRouters);
  std::vector<NodeContainer> wifiApNodes (nBackboneRouters);
  std::vector<NodeContainer> wifiStaNodes (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      lanNodes[i].Create (nLanNodes);
      wifiApNodes[i].Create (1);
      wifiStaNodes[i].Create (nWifiStaNodes);
    }

  // Create backbone ad hoc WiFi network
  YansWifiChannelHelper backboneChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper backbonePhy = YansWifiPhyHelper::Default ();
  backbonePhy.SetChannel (backboneChannel.Create ());

  WifiHelper backboneWifi;
  backboneWifi.SetStandard (WIFI_STANDARD_80211b);
  WifiMacHelper backboneMac;
  backboneMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer backboneDevices = backboneWifi.Install (backbonePhy, backboneMac, backboneRouters);

  // Set up LANs via CSMA for each backbone router
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  std::vector<NetDeviceContainer> lanDevices (nBackboneRouters);
  std::vector<Ptr<Node>> lanBridges (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      NodeContainer lanNet;
      lanNet.Add (backboneRouters.Get (i));
      lanNet.Add (lanNodes[i]);
      lanDevices[i] = csma.Install (lanNet);
    }

  // Infrastructure networks: one AP per backbone router + STAs
  YansWifiChannelHelper infraChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper infraPhy = YansWifiPhyHelper::Default ();
  infraPhy.SetChannel (infraChannel.Create ());

  WifiHelper infraWifi;
  infraWifi.SetStandard (WIFI_STANDARD_80211g);
  WifiMacHelper infraMac;

  std::vector<NetDeviceContainer> infraApDevices (nBackboneRouters);
  std::vector<NetDeviceContainer> infraStaDevices (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      Ssid ssid = Ssid ("infra-ssid-" + std::to_string (i));
      infraMac.SetType ("ns3::StaWifiMac",
                        "Ssid", SsidValue (ssid),
                        "ActiveProbing", BooleanValue (false));
      infraStaDevices[i] = infraWifi.Install (infraPhy, infraMac, wifiStaNodes[i]);

      infraMac.SetType ("ns3::ApWifiMac",
                        "Ssid", SsidValue (ssid));
      infraApDevices[i] = infraWifi.Install (infraPhy, infraMac, wifiApNodes[i]);
    }

  // Connect each AP to its backbone router using a virtual node (since AP needs to be separate from router)
  // Instead, connect the AP device directly to the router for code simplicity
  // Here, we'll "move" AP nodes to backboneRouters[i] (reuse node pointer)
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      // Move device to backbone router (OK as we created 1 AP node per backbone router)
      wifiApNodes[i].Get (0)->AddDevice (infraApDevices[i].Get (0));
      // No need for bridging, since AP device is part of the router node
    }

  // Internet stack
  OlsrHelper olsr;
  InternetStackHelper stack;
  // Enable OLSR on backbone routers only
  stack.SetRoutingHelper (olsr);
  stack.Install (backboneRouters);
  // Add only IPv4 on others (no OLSR)
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      stack.Install (lanNodes[i]);
      stack.Install (wifiApNodes[i]);
      stack.Install (wifiStaNodes[i]);
    }

  // IP address assignment
  Ipv4AddressHelper address;

  // Backbone ad hoc WiFi
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneIfs = address.Assign (backboneDevices);

  // CSMA LANs
  std::vector<Ipv4InterfaceContainer> lanIfs (nBackboneRouters);
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      std::string net = "10.2." + std::to_string (i) + ".0";
      address.SetBase (c_str(net), "255.255.255.0");
      lanIfs[i] = address.Assign (lanDevices[i]);
    }

  // Infrastructure WiFi
  std::vector<Ipv4InterfaceContainer> infraApIfs (nBackboneRouters);
  std::vector<Ipv4InterfaceContainer> infraStaIfs (nBackboneRouters);
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      std::string net = "10.3." + std::to_string (i) + ".0";
      address.SetBase (c_str(net), "255.255.255.0");
      infraStaIfs[i] = address.Assign (infraStaDevices[i]);
      // Assign to AP too (even if redundant)
      infraApIfs[i] = address.Assign (infraApDevices[i]);
    }

  // Mobility
  MobilityHelper mobility;
  // For backbone routers: static grid
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (10.0),
                                "MinY", DoubleValue (50.0),
                                "DeltaX", DoubleValue (50.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (nBackboneRouters),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (backboneRouters);

  // LAN and AP nodes: random around each router
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      MobilityHelper lanMob;
      lanMob.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                  "X", DoubleValue (10.0 + i * 50.0),
                                  "Y", DoubleValue (35.0),
                                  "Rho", StringValue ("ns3::UniformRandomVariable[Min=5|Max=10]"));
      lanMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      lanMob.Install (lanNodes[i]);
      lanMob.Install (wifiApNodes[i]);
    }

  // Wireless STAs: random walk 2D in area, for each infra network
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      MobilityHelper staMob;
      staMob.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                  "X", DoubleValue (10.0 + i * 50.0),
                                  "Y", DoubleValue (10.0),
                                  "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=20]"));
      staMob.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", TimeValue (Seconds (2.0)),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                               "Bounds", RectangleValue (Rectangle (10.0 + i*50.0 - 20, 10.0 + i*50.0 + 20, -10.0, 50.0)));
      staMob.Install (wifiStaNodes[i]);
    }

  // Enable CourseChange tracing
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeCallback (&CourseChangeCallback));

  // UDP flow: from a LAN node from the first LAN (nBackboneRouters > 1), to a WiFi STA from the last infrastructure
  uint16_t port = 9000;
  Ptr<Node> srcLanNode = lanNodes.front ().Get (0);
  Ptr<Node> dstStaNode = wifiStaNodes.back ().Get (0);

  // Find destination address (first interface on STA node)
  Ptr<Ipv4> ipv4 = dstStaNode->GetObject<Ipv4> ();
  Ipv4Address dstAddr = ipv4->GetAddress (1, 0).GetLocal ();

  // Setup UDP server/sink on STA node
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (dstStaNode);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));

  // Setup UDP client on src LAN node
  UdpClientHelper client (dstAddr, port);
  client.SetAttribute ("MaxPackets", UintegerValue (10000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (srcLanNode);
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  // Enable Pcap Tracing
  if (enableTracing)
    {
      csma.EnablePcapAll ("mixed-wired-wireless-csma");
      backbonePhy.EnablePcap ("mixed-wired-wireless-backbone-adhoc", backboneDevices);
      infraPhy.EnablePcap ("mixed-wired-wireless-infra", infraStaDevices.back ().Get (0));
      AnimationInterface anim ("mixed-wired-wireless.xml");
    }

  // Flow Monitor (optional)
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  monitor->SerializeToXmlFile ("mixed-wired-wireless.flowmon", true, true);
  Simulator::Destroy ();

  return 0;
}