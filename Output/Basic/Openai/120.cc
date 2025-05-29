#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvGrid");

void PrintRoutingTables (Ptr<Node> node, Ptr<Ipv4RoutingHelper> routingHelper)
{
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  for (uint32_t j = 0; j < ipv4->GetNInterfaces (); ++j)
    {
      Ipv4Address addr = ipv4->GetAddress (j, 0).GetLocal ();
      if (addr != Ipv4Address ("127.0.0.1"))
        {
          std::cout << "Node " << node->GetId () << " IP: " << addr << std::endl;
        }
    }
}

void PrintAllRoutingTables (NodeContainer nodes, Ipv4RoutingHelper& routingHelper)
{
  std::cout << "Printing Routing Tables at " << Simulator::Now ().GetSeconds () << "s:\n";
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
      if (proto)
        {
          proto->PrintRoutingTable (Create<OutputStreamWrapper> (&std::cout));
        }
    }
  std::cout << "--------------------------------------------\n";
}

int main (int argc, char *argv[])
{
  uint32_t nodeCount = 10;
  uint32_t gridWidth = 5;
  double step = 50.0;
  double nodeSpeed = 10.0;
  double areaWidth = 250.0, areaHeight = 100.0;
  double simTime = 50.0;
  uint32_t packetSize = 64;
  double echoInterval = 4.0;
  bool printRouting = true;
  double routingPrintInterval = 10.0;

  CommandLine cmd;
  cmd.AddValue ("printRouting", "Print routing tables", printRouting);
  cmd.AddValue ("routingInterval", "Interval for printing routing tables", routingPrintInterval);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (nodeCount);

  // Wifi device in ad-hoc mode
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility: grid + random walk (random within area)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (step),
                                 "DeltaY", DoubleValue (step),
                                 "GridWidth", UintegerValue (gridWidth),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", TimeValue (Seconds (2.0)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
                             "Bounds", RectangleValue (Rectangle (0.0, areaWidth, 0.0, areaHeight)));
  mobility.Install (nodes);

  // Install Internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Echo Servers + Clients in circular fashion
  ApplicationContainer serverApps, clientApps;
  uint16_t portBase = 9;

  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      // UDP Echo Server on each node
      UdpEchoServerHelper echoServer (portBase);
      ApplicationContainer serverApp = echoServer.Install (nodes.Get (i));
      serverApp.Start (Seconds (1.0));
      serverApp.Stop (Seconds (simTime));
      serverApps.Add (serverApp);

      // UDP Echo Client sending to next node (last -> first)
      Ipv4Address dstAddr = interfaces.GetAddress ((i + 1)%nodeCount);
      UdpEchoClientHelper echoClient (dstAddr, portBase);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t ((simTime-2.0)/echoInterval) + 2));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (echoInterval)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApp = echoClient.Install (nodes.Get (i));
      clientApp.Start (Seconds (2.0));
      clientApp.Stop (Seconds (simTime));
      clientApps.Add (clientApp);
    }

  // Enable PCAP tracing
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcap ("adhoc-aodv-grid", devices);

  // Optional periodic printing of routing tables
  if (printRouting)
    {
      Ptr<Ipv4RoutingHelper> helper = aodv.Copy ();
      for (double t = 4.0; t < simTime; t += routingPrintInterval)
        {
          Simulator::Schedule (Seconds (t),
            &PrintAllRoutingTables, nodes, aodv);
        }
    }

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Animation output
  AnimationInterface anim ("adhoc-aodv-grid.xml");
  for (uint32_t i = 0; i < nodeCount; ++i)
    anim.UpdateNodeDescription (nodes.Get (i), "Node" + std::to_string (i));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Flow monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint32_t totTxPackets = 0, totRxPackets = 0;
  double totDelay = 0.0;
  double totThroughput = 0.0;

  for (const auto& flow : stats)
    {
      auto t = classifier->FindFlow (flow.first);
      if ((t.destinationPort == portBase) &&
           !Ipv4Address (t.destinationAddress).IsBroadcast ())
        {
          totTxPackets += flow.second.txPackets;
          totRxPackets += flow.second.rxPackets;
          totDelay += flow.second.delaySum.GetSeconds ();
          double duration = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds ();
          if (duration > 0 && flow.second.rxBytes > 0)
            totThroughput += (flow.second.rxBytes * 8.0 / duration) / 1000.0; // kbps
        }
    }

  double pdr = (totTxPackets > 0) ? (double)totRxPackets / totTxPackets * 100.0 : 0.0;
  double avgDelay = (totRxPackets > 0) ? totDelay / totRxPackets : 0.0;
  double avgThroughput = totThroughput;

  std::cout << std::fixed << std::setprecision (3);
  std::cout << "\n==== Simulation Results ====" << std::endl;
  std::cout << "Total Tx Packets:      " << totTxPackets << std::endl;
  std::cout << "Total Rx Packets:      " << totRxPackets << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
  std::cout << "Avg End-to-End Delay:  " << avgDelay << " s" << std::endl;
  std::cout << "Avg Throughput:        " << avgThroughput << " kbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}