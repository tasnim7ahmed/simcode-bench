/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MANET simulation with AODV, random mobility, and UDP traffic
 * Throughput and packet loss monitoring
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvManetExample");

int main (int argc, char *argv[])
{
  // Configuration variables
  double simTime = 30.0;   // Simulation time [seconds]
  uint32_t packetSize = 512;    // Packet size [Bytes]
  uint32_t numPackets = 2000;
  double nodeSpeed = 10.0; // m/s
  double pauseTime = 2.0;  // [s]
  uint32_t numNodes = 4;

  // Enable logging for debugging if needed
  //LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Setup Wi-Fi channel
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility: RandomWaypointMobilityModel within a 100x100 area
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
    "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
    "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
    "PositionAllocator", StringValue (
      "ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=100.0|MaxY=100.0]")
  );
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
    "MinX", DoubleValue (0.0),
    "MinY", DoubleValue (0.0),
    "MaxX", DoubleValue (100.0),
    "MaxY", DoubleValue (100.0)
  );
  mobility.Install (nodes);

  // Install Internet stack
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Set up UDP traffic: node 0 -> node 1
  uint16_t serverPort = 4000;

  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApp = udpServer.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  UdpClientHelper udpClient (interfaces.GetAddress (1), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.05))); // 20 pkts/sec
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = udpClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // Enable pcap tracing
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcapAll ("aodv-manet");

  // FlowMonitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // FlowMonitor statistics
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();

  uint32_t totalTxPackets = 0;
  uint32_t totalRxPackets = 0;
  uint64_t totalRxBytes = 0;

  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);

      // Print applications only
      if ((t.sourceAddress == interfaces.GetAddress (0) && t.destinationAddress == interfaces.GetAddress (1))
        || (t.sourceAddress == interfaces.GetAddress (1) && t.destinationAddress == interfaces.GetAddress (0)))
        {
          std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
          std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
          std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
          double throughput = flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1024.0;
          std::cout << "  Throughput: " << throughput << " Kbps\n";
          totalTxPackets += flow.second.txPackets;
          totalRxPackets += flow.second.rxPackets;
          totalRxBytes += flow.second.rxBytes;
        }
    }
  std::cout << "======= Total Summary =======\n";
  std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
  std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
  std::cout << "Total Lost Packets: " << totalTxPackets - totalRxPackets << "\n";
  double totalThroughput = totalRxBytes * 8.0 / (simTime - 2.0) / 1024.0;
  std::cout << "Aggregate throughput: " << totalThroughput << " Kbps\n";

  Simulator::Destroy ();
  return 0;
}