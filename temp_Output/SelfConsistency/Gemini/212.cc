#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhoc");

int
main (int argc, char *argv[])
{
  bool enablePcap = true;
  uint32_t numNodes = 5;
  double simulationTime = 10.0;
  std::string appDataRate = "1Mbps";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable/disable PCAP traces", enablePcap);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("appDataRate", "Application data rate", appDataRate);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("AodvRoutingProtocol", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType (WifiMacHelper::ADHOC,
               "Ssid", Ssid ("ns-3-adhoc"));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  Ipv4ListRoutingHelper list;
  list.Add (aodv, 0);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Setup UDP client/server
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (numNodes - 1)); // Last node as server
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1.0));

  UdpClientHelper client (interfaces.GetAddress (numNodes - 1), port); // Send to the server address
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01))); // packets/sec
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetDataRate (DataRate (appDataRate));
  ApplicationContainer clientApps = client.Install (nodes.Get (0)); // First node as client
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2.0));

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, 100, 100)));
  mobility.Install (nodes);

  // Enable PCAP tracing
  if (enablePcap)
    {
      phy.EnablePcapAll ("aodv-adhoc");
    }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance ();
  if (globalRouting != nullptr)
    {
      globalRouting->PrintRoutingTableAllAt (Seconds (simulationTime - 1));
    }

  Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
  if (ipv4_0 != nullptr)
    {
      ipv4_0->GetRoutingProtocol()->PrintRoutingTable (Create<OutputStreamWrapper> ("aodv-routing-table-node0.txt", std::ios::out));
    }

  Ptr<Ipv4> ipv4_last = nodes.Get(numNodes - 1)->GetObject<Ipv4>();
  if (ipv4_last != nullptr)
    {
      ipv4_last->GetRoutingProtocol()->PrintRoutingTable (Create<OutputStreamWrapper> ("aodv-routing-table-nodelast.txt", std::ios::out));
    }

  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      std::cout << "Flow ID: " << i->first << std::endl;
      std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
      std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps" << std::endl;
    }

  Simulator::Destroy ();

  return 0;
}