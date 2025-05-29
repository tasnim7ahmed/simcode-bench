#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  double interval = 0.01; // seconds
  uint32_t numPackets = 0; // unlimited
  double simulationTime = 10.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("interval", "Inter-packet interval (s)", interval);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  // Set up Wifi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"),
                                "ControlMode", StringValue ("DsssRate1Mbps"));

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Place the nodes in a line
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (100.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Enable routing on the middle node
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Application setup: CBR source on node 0 -> sink on node 2
  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (2), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 1));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ((packetSize * 8) / interval)));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
  onoff.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited

  ApplicationContainer apps = onoff.Install (nodes.Get (0));

  // Tracing: enable PHY and MAC packet traces to file
  phy.EnablePcap ("wifi-simple-line", devices);

  // Flow monitor setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  monitor->SerializeToXmlFile ("throughput-results.xml", true, true);

  Simulator::Destroy ();
  return 0;
}