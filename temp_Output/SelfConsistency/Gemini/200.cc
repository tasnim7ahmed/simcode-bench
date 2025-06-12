#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool enableNetAnim = true;
  double simulationTime = 10.0;
  std::string phyMode ("DsssRate1Mbps");
  uint32_t packetSize = 1024;
  uint32_t numNodes = 4;
  double distance = 10.0;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("distance", "Grid distance in meters", distance);
  cmd.AddValue ("verbose", "Turn on all WifiNetDevice log components", verbose);
  cmd.Parse (argc, argv);

  // Set up logging
  if (verbose)
    {
      LogComponentEnable ("AdhocAodvSimulation", LOG_LEVEL_INFO);
      LogComponentEnable ("WifiNetDevice", LOG_LEVEL_ALL);
      LogComponentEnable ("AdhocRoutingProtocol", LOG_LEVEL_ALL);
    }

  // Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Set up Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("Model", StringValue ("ns3::ConstantSpeedPropagationDelayModel"));
  wifiChannel.AddPropagationLoss ("Model", StringValue ("ns3::FriisPropagationLossModel"));
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Set mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("200.0"),
                                 "Y", StringValue ("200.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
                             "Bounds", StringValue ("0 400 0 400"));
  mobility.Install (nodes);

  // Install AODV routing
  AodvHelper aodv;
  Ipv4ListRoutingHelper list;
  list.Add (aodv, 0);
  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  // Assign addresses
  Ipv4AddressHelper ipv4Addresses;
  ipv4Addresses.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4Addresses.Assign (devices);

  // Create Applications (UDP traffic)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (numNodes - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (numNodes - 1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable NetAnim
  if (enableNetAnim)
    {
      AnimationInterface anim ("aodv-animation.xml");
      anim.SetMaxPktsPerTraceFile (1000000);
    }

  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print Flowmonitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Packet Delivery Ratio: " << (double) i->second.rxPackets / (double) i->second.txPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
       if (i->second.rxPackets > 0) {
            std::cout << "  Average End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
        } else {
            std::cout << "  Average End-to-End Delay: N/A (No packets received)\n";
        }
    }

  Simulator::Destroy ();

  return 0;
}