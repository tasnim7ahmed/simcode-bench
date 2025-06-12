/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeWifiLineThroughput");

void
RxPacketTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &address)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Simulation parameters
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 1000;
  double interval = 0.01; // seconds, 100 packets/sec
  double simulationTime = 15.0;

  // Logging
  LogComponentEnable ("ThreeNodeWifiLineThroughput", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Setup Wi-Fi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-line-topology");

  NetDeviceContainer devices;

  // We'll set up two WiFi networks where node 1 is in the middle and part of both
  // [0]<--WiFiNet1-->[1]<--WiFiNet2-->[2]
  // WiFiNet1: nodes 0-1
  // WiFiNet2: nodes 1-2

  NodeContainer wifiNet1;
  wifiNet1.Add(nodes.Get(0));
  wifiNet1.Add(nodes.Get(1));
  NetDeviceContainer devices1 = wifi.Install (phy, mac, wifiNet1);

  NodeContainer wifiNet2;
  wifiNet2.Add(nodes.Get(1));
  wifiNet2.Add(nodes.Get(2));
  NetDeviceContainer devices2 = wifi.Install (phy, mac, wifiNet2);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0)); // middle node, 50m away
  positionAlloc->Add (Vector (100.0, 0.0, 0.0)); // node2, 100m away
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  // subnet 1: 10.1.1.0
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  // subnet 2: 10.1.2.0
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Application: CBR source (OnOffApplication) from node 0 to node 2
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces2.GetAddress (1), sinkPort)); // node 2's address on subnet 2

  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetConstantRate (DataRate (static_cast<uint64_t>(packetSize * 8.0 / interval)), packetSize);
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
  ApplicationContainer clientApp = onoff.Install (nodes.Get (0));

  // Tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> txStream = ascii.CreateFileStream ("tx-trace.txt");
  Ptr<OutputStreamWrapper> rxStream = ascii.CreateFileStream ("rx-trace.txt");

  devices1.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeBoundCallback (&RxPacketTrace, txStream));
  devices2.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeBoundCallback (&RxPacketTrace, rxStream));

  // Flow monitor for throughput measurement
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Throughput calculation
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::ofstream outFile ("throughput.txt");
  outFile << "FlowID\tSrcAddr\tDstAddr\tTxBytes\tRxBytes\tThroughput(Mbps)" << std::endl;

  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

      double throughput = iter->second.rxBytes * 8.0 /
        (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()) / 1e6; // Mbps

      outFile << iter->first << "\t"
              << t.sourceAddress << "\t" << t.destinationAddress << "\t"
              << iter->second.txBytes << "\t" << iter->second.rxBytes << "\t"
              << throughput << std::endl;
    }
  outFile.close();

  Simulator::Destroy ();
  return 0;
}