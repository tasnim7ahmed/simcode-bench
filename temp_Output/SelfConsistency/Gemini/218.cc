#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DsdvVanetSimulation");

int main (int argc, char *argv[])
{
  bool enableNetAnim = true;
  bool enableFlowMonitor = true;
  std::string phyMode ("DsssRate11Mbps");
  double simulationTime = 10; //seconds
  double distance = 50; //meters between nodes
  uint32_t numNodes = 10;

  CommandLine cmd;
  cmd.AddValue ("enableNetAnim", "Enable NetAnim animation output", enableNetAnim);
  cmd.AddValue ("enableFlowMonitor", "Enable FlowMonitor statistics", enableFlowMonitor);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes in meters", distance);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);

  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("vanet-network")));

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (numNodes),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<ConstantVelocityMobilityModel> mob = node->GetObject<ConstantVelocityMobilityModel> ();
      Vector velocity (1, 0, 0); // Move along X axis at 1 m/s
      mob->SetVelocity (velocity);
    }


  // WiFi configuration
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Install DSDV protocol
  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol ("ns3::DsdvRoutingProtocol", "RoutingTableSize", StringValue ("65536")); //Increase table size
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create applications:  For example, ping between node 0 and node numNodes-1
  uint16_t port = 9; // Discard port (RFC 863)
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (numNodes - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (numNodes - 1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01))); // packets/second
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // NetAnim
  if (enableNetAnim)
    {
      AnimationInterface anim ("dsdv-vanet.xml");
      for (uint32_t i = 0; i < numNodes; ++i)
        {
          anim.UpdateNodeDescription (nodes.Get (i), "VANET Node");
          anim.UpdateNodeColor (nodes.Get (i), 255, 0, 0); // Red
        }
      anim.EnablePacketMetadata (true);
    }

  // FlowMonitor
  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmon;
      Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
      Simulator::Stop (Seconds (simulationTime));
      Simulator::Run ();

      monitor->CheckForLostPackets ();

      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
      std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
        }

      monitor->SerializeToXmlFile("dsdv-vanet.flowmon", true, true);
    }
  else
    {
      Simulator::Stop (Seconds (simulationTime));
      Simulator::Run ();
    }

  Simulator::Destroy ();
  return 0;
}