#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifi2Nodes");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  uint32_t nNodes = 2;
  double simTime = 20.0;

  NodeContainer nodes;
  nodes.Create (nNodes);

  // Mobility: Random movement within bounds
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
    "Speed", StringValue ("ns3::UniformRandomVariable[Min=2.0|Max=5.0]"),
    "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
    "PositionAllocator", StringValue (
      "ns3::RandomRectanglePositionAllocator["
        "X=ns3::UniformRandomVariable[Min=0.0|Max=100.0],"
        "Y=ns3::UniformRandomVariable[Min=0.0|Max=100.0]"
      "]")
  );
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (10.0),
                                "MinY", DoubleValue (10.0),
                                "DeltaX", DoubleValue (50.0),
                                "DeltaY", DoubleValue (50.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  // Wifi: 802.11 ad hoc
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Internet stack + IP addressing
  InternetStackHelper internet;
  internet.Install (nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP Echo Server on each node
  uint16_t portA = 9000;
  uint16_t portB = 9001;
  UdpEchoServerHelper echoServerA (portA);
  ApplicationContainer serverAppsA = echoServerA.Install (nodes.Get (0));
  serverAppsA.Start (Seconds (1.0));
  serverAppsA.Stop (Seconds (simTime));
  UdpEchoServerHelper echoServerB (portB);
  ApplicationContainer serverAppsB = echoServerB.Install (nodes.Get (1));
  serverAppsB.Start (Seconds (1.0));
  serverAppsB.Stop (Seconds (simTime));

  // UDP Echo Client on opposing node to test bidirectional
  uint32_t packetSize = 1024;
  uint32_t nPackets = 12;
  double interval = 1.0;
  UdpEchoClientHelper echoClientA (interfaces.GetAddress (1), portB);
  echoClientA.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  echoClientA.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClientA.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientAppA = echoClientA.Install (nodes.Get (0));
  clientAppA.Start (Seconds (2.0));
  clientAppA.Stop (Seconds (simTime - 1));

  UdpEchoClientHelper echoClientB (interfaces.GetAddress (0), portA);
  echoClientB.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  echoClientB.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClientB.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientAppB = echoClientB.Install (nodes.Get (1));
  clientAppB.Start (Seconds (2.5));
  clientAppB.Stop (Seconds (simTime - 1));

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // NetAnim
  AnimationInterface anim ("adhoc-2nodes.xml");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Gather FlowMonitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalTx = 0, totalRx = 0;
  double totalDelay = 0;
  uint64_t totalRxBytes = 0;
  double firstTx = 0, lastRx = 0;
  uint32_t nFlows = 0;

  std::cout << "Flow statistics:" << std::endl;
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
      std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
      std::cout << "  Packet Delivery Ratio: " << (iter->second.rxPackets * 100.0 / std::max (1u, iter->second.txPackets)) << " %" << std::endl;
      double avgDelay = (iter->second.rxPackets > 0) ? (iter->second.delaySum.GetSeconds () / iter->second.rxPackets) : 0.0;
      std::cout << "  Mean Delay: " << avgDelay << " s" << std::endl;
      double throughput = (iter->second.rxBytes * 8.0) / (simTime - 2.0) / 1000; // kbps
      std::cout << "  Throughput: " << throughput << " kbps" << std::endl;

      totalTx += iter->second.txPackets;
      totalRx += iter->second.rxPackets;
      totalDelay += iter->second.delaySum.GetSeconds ();
      totalRxBytes += iter->second.rxBytes;
      if (iter->second.timeFirstTxPacket.GetSeconds () > 0 && (firstTx == 0 || iter->second.timeFirstTxPacket.GetSeconds () < firstTx)) firstTx = iter->second.timeFirstTxPacket.GetSeconds ();
      if (iter->second.timeLastRxPacket.GetSeconds () > 0 && iter->second.timeLastRxPacket.GetSeconds () > lastRx) lastRx = iter->second.timeLastRxPacket.GetSeconds ();
      ++nFlows;
    }
  double pdr = (totalRx * 100.0) / std::max (1.0, totalTx);
  double meanDelay = (totalRx > 0) ? (totalDelay / totalRx) : 0.0;
  double throughput = (lastRx > firstTx) ? (totalRxBytes * 8.0 / (lastRx - firstTx) / 1000) : 0.0; // kbps

  std::cout << "\n======== Aggregate Results ========" << std::endl;
  std::cout << "Total Flows: " << nFlows << std::endl;
  std::cout << "Aggregate PDR: " << pdr << " %" << std::endl;
  std::cout << "Aggregate Mean Delay: " << meanDelay << " s" << std::endl;
  std::cout << "Aggregate Throughput: " << throughput << " kbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}