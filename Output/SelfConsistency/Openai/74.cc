/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * MANET Routing Protocol Comparison Simulation in ns-3
 * 
 * Allows configuration of protocol, number of nodes, simulation duration,
 * area size, node speed, transmission power, and output/trace options.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/dsr-main-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingProtocolsComparison");

struct FlowStats
{
  uint32_t flowId;
  std::string protocol;
  uint32_t txPackets;
  uint32_t rxPackets;
  uint64_t txBytes;
  uint64_t rxBytes;
  double   delaySum;
  double   jitterSum;
  double   offeredThroughput;
  double   achievedThroughput;
};

void
WriteStatsToCsv (const std::string &filename, const std::vector<FlowStats> &stats)
{
  std::ofstream out (filename, std::ios_base::out);
  if (!out.is_open ())
    {
      NS_LOG_ERROR ("Cannot open CSV file " << filename << " for writing!");
      return;
    }

  out << "FlowId,Protocol,TxPackets,RxPackets,TxBytes,RxBytes,DelaySum(s),JitterSum(s),OfferedThroughput(bps),AchievedThroughput(bps)\n";
  for (const auto& stat : stats)
    {
      out << stat.flowId << ',' 
          << stat.protocol << ','
          << stat.txPackets << ',' 
          << stat.rxPackets << ',' 
          << stat.txBytes << ',' 
          << stat.rxBytes << ','
          << std::fixed << std::setprecision(6) << stat.delaySum << ','
          << std::fixed << std::setprecision(6) << stat.jitterSum << ','
          << std::fixed << std::setprecision(2) << stat.offeredThroughput << ','
          << std::fixed << std::setprecision(2) << stat.achievedThroughput << '\n';
    }
  out.close ();
}

int
main (int argc, char *argv[])
{
  // Default simulation parameters
  std::string protocol = "AODV";
  uint32_t nNodes = 50;
  double nodeSpeed = 20.0; // m/s
  double simTime = 200.0;  // seconds
  double txp = 7.5;        // dBm
  uint32_t nFlows = 10;
  bool printMobility = false;
  uint32_t areaWidth = 1500;
  uint32_t areaHeight = 300;
  std::string csvFile = "manet-stats.csv";
  bool enableFlowMonitor = true;

  CommandLine cmd;
  cmd.AddValue("protocol", "Routing protocol: DSDV, AODV, OLSR, or DSR", protocol);
  cmd.AddValue("nNodes", "Number of mobile nodes", nNodes);
  cmd.AddValue("nodeSpeed", "Speed of mobile nodes (m/s)", nodeSpeed);
  cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue("txp", "Transmission power (dBm)", txp);
  cmd.AddValue("nFlows", "Number of UDP flows (source/sink pairs)", nFlows);
  cmd.AddValue("areaWidth", "Width of the rectangular area (meters)", areaWidth);
  cmd.AddValue("areaHeight", "Height of the rectangular area (meters)", areaHeight);
  cmd.AddValue("csvFile", "CSV output file for results", csvFile);
  cmd.AddValue("printMobility", "Enable mobility tracing (CSV format)", printMobility);
  cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor output", enableFlowMonitor);
  cmd.Parse(argc, argv);

  NS_LOG_UNCOND("MANET Routing Protocol: " << protocol);
  NS_LOG_UNCOND("Nodes: " << nNodes << ", Area: " << areaWidth << "x" << areaHeight << " m, NodeSpeed: " << nodeSpeed << " m/s");
  NS_LOG_UNCOND("SimTime: " << simTime << " s, Flows: " << nFlows << ", TxPower: " << txp << " dBm");

  // Create nodes
  NodeContainer nodes;
  nodes.Create (nNodes);

  // Configure WiFi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  // Set Tx power
  wifiPhy.Set ("TxPowerStart", DoubleValue (txp));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txp));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-89.0)); // in dBm (typical for 802.11b)
  wifiPhy.Set ("RxGain", DoubleValue (0));

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(nodeSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0]"),
                             "PositionAllocator", StringValue (
                                 "ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=" + std::to_string(areaWidth) + "|MaxY=" + std::to_string(areaHeight) + "]"));
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "MaxX", DoubleValue (areaWidth),
                                  "MaxY", DoubleValue (areaHeight));
  mobility.Install (nodes);

  // Optional mobility tracing
  std::ofstream mobilityTrace;
  if (printMobility)
    {
      mobilityTrace.open ("mobility-trace.csv", std::ios_base::out);
      if (!mobilityTrace)
        {
          NS_LOG_ERROR ("Unable to open mobility trace file.");
          return 1;
        }
      mobilityTrace << "Time,NodeId,X,Y\n";
    }

  // Periodic logging of node mobility
  if (printMobility)
    {
      Simulator::Schedule (Seconds (0.0), [&nodes, &mobilityTrace] () {
          for (uint32_t i = 0; i < nodes.GetN (); ++i)
            {
              Ptr<Node> node = nodes.Get (i);
              Ptr<MobilityModel> mm = node->GetObject<MobilityModel> ();
              if (mm)
                {
                  Vector pos = mm->GetPosition ();
                  mobilityTrace << Simulator::Now ().GetSeconds () << ","
                                << node->GetId () << ","
                                << pos.x << ","
                                << pos.y << "\n";
                }
            }
          Simulator::Schedule (Seconds (1.0), [&nodes, &mobilityTrace] () { // period = 1s
              for (uint32_t i = 0; i < nodes.GetN (); ++i)
                {
                  Ptr<Node> node = nodes.Get (i);
                  Ptr<MobilityModel> mm = node->GetObject<MobilityModel> ();
                  if (mm)
                    {
                      Vector pos = mm->GetPosition ();
                      mobilityTrace << Simulator::Now ().GetSeconds () << ","
                                    << node->GetId () << ","
                                    << pos.x << ","
                                    << pos.y << "\n";
                    }
                }
              Simulator::Schedule (Seconds (1.0), [&nodes, &mobilityTrace] () { }); // Dummy: will be rescheduled later          
            });
        });
    }

  // Routing
  InternetStackHelper internet;
  Ipv4ListRoutingHelper list;
  DsdvHelper dsdv;
  AodvHelper aodv;
  OlsrHelper olsr;
  DsrMainHelper dsrMain;
  DsrHelper dsr;

  protocol = std::string (protocol.begin (), std::remove (protocol.begin (), protocol.end (), ' ')); // Trim whitespace

  if (protocol == "DSDV")
    {
      list.Add (dsdv, 0);
      internet.SetRoutingHelper (list);
      internet.Install (nodes);
    }
  else if (protocol == "AODV")
    {
      list.Add (aodv, 0);
      internet.SetRoutingHelper (list);
      internet.Install (nodes);
    }
  else if (protocol == "OLSR")
    {
      list.Add (olsr, 0);
      internet.SetRoutingHelper (list);
      internet.Install (nodes);
    }
  else if (protocol == "DSR")
    {
      internet.Install (nodes);
      dsrMain.Install (dsr, nodes);
    }
  else
    {
      NS_LOG_ERROR ("Unsupported protocol: " << protocol);
      return 1;
    }

  // IP Assignment
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP traffic configuration
  uint16_t port = 9000;
  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable> ();
  rv->SetAttribute ("Min", DoubleValue (50.0));
  rv->SetAttribute ("Max", DoubleValue (51.0));

  ApplicationContainer serverApps, clientApps;

  std::vector<uint32_t> selectedNodes;
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      selectedNodes.push_back(i);
    }
  // Shuffle for randomness in selecting distinct src/dst
  std::random_shuffle (selectedNodes.begin (), selectedNodes.end ());

  for (uint32_t i = 0; i < nFlows; ++i)
    {
      uint32_t srcIdx = selectedNodes[i];
      uint32_t dstIdx = selectedNodes[(i + nFlows) % nNodes];
      if (srcIdx == dstIdx)
        dstIdx = (dstIdx + 1) % nNodes; // avoid src==dst

      // Sink App (on dst)
      UdpServerHelper server (port+i);
      ApplicationContainer sinkApp = server.Install (nodes.Get (dstIdx));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simTime));
      serverApps.Add (sinkApp);

      // Client App (on src)
      UdpClientHelper client (interfaces.GetAddress(dstIdx), port+i);
      client.SetAttribute ("MaxPackets", UintegerValue (2000000));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.02))); // 50 packets/sec
      client.SetAttribute ("PacketSize", UintegerValue (512));

      double appStart = rv->GetValue ();
      ApplicationContainer clientApp = client.Install (nodes.Get (srcIdx));
      clientApp.Start (Seconds (appStart));
      clientApp.Stop (Seconds (simTime));
      clientApps.Add (clientApp);
    }

  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  if (enableFlowMonitor)
    {
      flowmon = flowmonHelper.InstallAll ();
    }

  // Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  if (printMobility)
    {
      mobilityTrace.close ();
    }

  // Collect results
  std::vector<FlowStats> results;

  if (enableFlowMonitor && flowmon)
    {
      flowmon->CheckForLostPackets ();
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats ();

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::string proto = protocol;
          uint32_t txPackets = i->second.txPackets;
          uint32_t rxPackets = i->second.rxPackets;
          uint64_t txBytes = i->second.txBytes;
          uint64_t rxBytes = i->second.rxBytes;
          double delaySum = i->second.delaySum.GetSeconds ();
          double jitterSum = i->second.jitterSum.GetSeconds ();
          double simulationTime = simTime;
          double offeredThroughput = txBytes * 8.0 / simulationTime; // (bps)
          double achievedThroughput = rxBytes * 8.0 / simulationTime; // (bps)

          // Store only flows with nonzero packets (filter out ARP/tcp flows etc)
          if ((t.destinationPort >= port) && (t.destinationPort < port + nFlows))
            {
              results.push_back ({i->first, proto, txPackets, rxPackets,
                                  txBytes, rxBytes, delaySum, jitterSum,
                                  offeredThroughput, achievedThroughput});
            }
        }
      flowmon->SerializeToXmlFile ("manet-flowmon.xml", true, true);
    }
  else
    {
      // If FlowMonitor is off, get stats from UDP sinks directly
      for (uint32_t i = 0; i < serverApps.GetN (); ++i)
        {
          Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps.Get (i));
          uint32_t rxPackets = server->GetReceived ();
          // We cannot get bytes/throughput/delay without FlowMonitor
          results.push_back ({i, protocol, 0, rxPackets, 0, 0, 0, 0, 0, 0});
        }
    }

  WriteStatsToCsv (csvFile, results);

  Simulator::Destroy ();
  NS_LOG_UNCOND ("Simulation completed. Results written to " << csvFile);
  return 0;
}