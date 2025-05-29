/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocNetAnimExample");

int main (int argc, char *argv[])
{
  uint32_t numNodes = 4;
  double simTime = 60.0; // seconds
  double areaSize = 100.0;
  std::string animFile = "aodv-adhoc-netanim.xml";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("areaSize", "Size of the area (meters)", areaSize);
  cmd.AddValue ("animFile", "NetAnim output file", animFile);
  cmd.Parse (argc, argv);

  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // 2. Set up Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // 3. Mobility - Random Waypoint Model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("Uniform(0," + std::to_string (int(areaSize)) + ")"),
                                "Y", StringValue ("Uniform(0," + std::to_string (int(areaSize)) + ")"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("Uniform(1.0,5.0)"),
                             "Pause", StringValue ("Constant:0.0"),
                             "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
  mobility.Install (nodes);

  // 4. Install Internet stack & AODV
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 5. Install UDP traffic (OnOff applications) between nodes
  uint16_t port = 5000;
  double startTime = 2.0;
  double stopTime = simTime - 2.0;
  ApplicationContainer appCont;

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      for (uint32_t j = 0; j < numNodes; ++j)
        {
          if (i == j) continue;

          OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (j), port));
          onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
          onoff.SetAttribute ("PacketSize", UintegerValue (512));
          onoff.SetAttribute ("StartTime", TimeValue (Seconds (startTime + i + j))); // Slightly stagger the start
          onoff.SetAttribute ("StopTime", TimeValue (Seconds (stopTime)));

          appCont.Add (onoff.Install (nodes.Get (i)));
        }
    }

  // Sink (PacketSink) on all nodes to receive UDP
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
      sink.SetAttribute ("StartTime", TimeValue (Seconds (0.0)));
      sink.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      appCont.Add (sink.Install (nodes.Get (i)));
    }

  // 6. Enable NetAnim visualization
  AnimationInterface anim (animFile);
  anim.SetMobilityPollInterval (Seconds (1.0));
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      std::ostringstream oss;
      oss << "Node" << i;
      anim.UpdateNodeDescription (i, oss.str ());
      anim.UpdateNodeColor (i, 0, 255, 0); // green nodes
    }

  // 7. FlowMonitor setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 8. Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // 9. Process FlowMonitor results
  double totalTxPackets = 0;
  double totalRxPackets = 0;
  double totalTxBytes = 0;
  double totalRxBytes = 0;
  double totalDelay = 0;
  uint32_t flowsWithRx = 0;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (auto const &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);

      if (flow.second.rxPackets > 0)
        {
          flowsWithRx++;
          totalDelay += (double)flow.second.delaySum.GetSeconds () / flow.second.rxPackets;
        }

      totalTxPackets += flow.second.txPackets;
      totalRxPackets += flow.second.rxPackets;
      totalTxBytes += flow.second.txBytes;
      totalRxBytes += flow.second.rxBytes;
    }

  double pdr = (totalRxPackets / totalTxPackets) * 100.0;
  double throughput = (totalRxBytes * 8.0) / (simTime * 1000000.0); // Mbps
  double avgDelay = flowsWithRx > 0 ? (totalDelay / flowsWithRx) : 0;

  std::cout << "==== Simulation Results ====" << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
  std::cout << "===========================" << std::endl;

  Simulator::Destroy ();
  return 0;
}