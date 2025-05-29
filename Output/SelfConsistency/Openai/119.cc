#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridRingEcho");

int
main (int argc, char *argv[])
{
  // Set logging
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  uint32_t nNodes = 6;
  double simTime = 20.0;

  // Create nodes
  NodeContainer nodes;
  nodes.Create (nNodes);

  // Set up WiFi (802.11b as default)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
  wifiPhy.Set ("RxGain", DoubleValue (0));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility: Arrange nodes in a 2x3 grid inside a box [0, 0] - [60, 40]
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (10.0),
                                "MinY", DoubleValue (10.0),
                                "DeltaX", DoubleValue (30.0),
                                "DeltaY", DoubleValue (20.0),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));

  // Allow for random walks within a fixed bound
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 60, 0, 40)),
                             "Distance", DoubleValue (10.0),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Time", TimeValue (Seconds (1.0)));
  mobility.Install (nodes);

  // Install Internet stack & assign IP addresses
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP echo servers (port 9 as default)
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      UdpEchoServerHelper server (9);
      serverApps.Add (server.Install (nodes.Get (i)));
    }
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  // Install UDP echo clients in a ring topology (node i sends to node (i+1)%nNodes)
  ApplicationContainer clientApps;
  uint32_t maxPackets = 20;
  double interval = 0.5; // seconds
  uint16_t serverPort = 9;
  uint32_t packetSize = 64;

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t destIdx = (i + 1) % nNodes;
      UdpEchoClientHelper client (interfaces.GetAddress (destIdx), serverPort);
      client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
      client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));
      clientApps.Add (client.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simTime));

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Animation
  AnimationInterface anim ("adhoc-grid-ring-echo.xml");
  anim.SetMobilityPollInterval (Seconds (1.0));
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      std::ostringstream oss;
      oss << "Node-" << i;
      anim.UpdateNodeDescription (nodes.Get (i), oss.str ());
      anim.UpdateNodeColor (nodes.Get (i), 255, 0, 0); // Red
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint32_t totalRxPackets = 0;
  uint32_t totalTxPackets = 0;
  uint64_t totalRxBytes = 0;
  uint64_t totalTxBytes = 0;
  double totalDelay = 0.0;
  uint32_t nFlows = 0;

  std::cout << "\n=============== Flow Monitor Results ===============\n";
  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if (t.destinationPort != serverPort)
        continue; // Only include echo traffic

      std::cout << "Flow ID: " << flow.first
                << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Tx Bytes: " << flow.second.txBytes << "\n";
      std::cout << "  Rx Bytes: " << flow.second.rxBytes << "\n";
      double pdr = (flow.second.txPackets) ? double (flow.second.rxPackets) / flow.second.txPackets : 0.0;
      std::cout << "  Packet Delivery Ratio: " << pdr * 100 << "%\n";
      double throughput = (flow.second.rxBytes * 8.0) / (simTime - 2.0) / 1000; // Kbps (excluding startup)
      std::cout << "  Throughput: " << throughput << " Kbps\n";
      double delay = (flow.second.rxPackets ? (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) : 0.0);
      std::cout << "  Mean End-to-End Delay: " << delay * 1000 << " ms\n";
      std::cout << "--------------------------------------------------\n";

      totalTxPackets += flow.second.txPackets;
      totalRxPackets += flow.second.rxPackets;
      totalTxBytes += flow.second.txBytes;
      totalRxBytes += flow.second.rxBytes;
      totalDelay += flow.second.delaySum.GetSeconds ();
      nFlows++;
    }

  if (nFlows > 0)
    {
      double overallPdr = totalTxPackets ? (double) totalRxPackets / totalTxPackets : 0.0;
      double overallThroughput = (totalRxBytes * 8.0) / (simTime - 2.0) / 1000; // Kbps
      double meanDelay = totalRxPackets ? (totalDelay / totalRxPackets) : 0.0;
      std::cout << "\n=============== Aggregate Metrics ================\n";
      std::cout << "Total Flows:         " << nFlows << "\n";
      std::cout << "Total Tx Packets:    " << totalTxPackets << "\n";
      std::cout << "Total Rx Packets:    " << totalRxPackets << "\n";
      std::cout << "Aggregate PDR:       " << overallPdr * 100 << "%\n";
      std::cout << "Aggregate Throughput:" << overallThroughput << " Kbps\n";
      std::cout << "Mean End-to-End Delay: " << meanDelay * 1000 << " ms\n";
    }

  Simulator::Destroy ();
  return 0;
}