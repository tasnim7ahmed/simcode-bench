#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiBidirectionalEcho");

int
main (int argc, char *argv[])
{
  // Simulation parameters
  uint32_t numNodes = 2;
  double simTime = 60.0; // seconds
  double txPowerDbm = 50.0;
  double nodeSpeed = 2.0; // m/s
  double pauseTime = 0.5; // seconds
  uint32_t maxPackets = 50;
  double interval = 0.5; // seconds between packets

  // Enable logging for debugging (comment out if not needed)
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // WiFi configuration in Ad Hoc mode
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-99.0)); // Ensures detection at high power
  wifiPhy.Set ("RxGain", DoubleValue(0));
  wifiPhy.Set ("TxGain", DoubleValue(0));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility: RandomWalk2dMobilityModel within boundaries
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Distance", DoubleValue (10.0));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (10.0),
                                "MinY", DoubleValue (10.0),
                                "DeltaX", DoubleValue (60.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  // Install Internet stack and assign IP addresses
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP Echo server on both nodes
  uint16_t portNode0 = 9;
  uint16_t portNode1 = 10;

  UdpEchoServerHelper echoServer0 (portNode0);
  UdpEchoServerHelper echoServer1 (portNode1);

  ApplicationContainer serverApps0 = echoServer0.Install (nodes.Get (0));
  serverApps0.Start (Seconds (1.0));
  serverApps0.Stop (Seconds (simTime));

  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (1));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (simTime));

  // Install client apps: node1->node0, node0->node1
  UdpEchoClientHelper echoClient0 (interfaces.GetAddress (1), portNode1); // node0 targets node1
  echoClient0.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient0.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient0.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps0 = echoClient0.Install (nodes.Get(0));
  clientApps0.Start (Seconds (2.0));
  clientApps0.Stop (Seconds (simTime - 1.0));

  UdpEchoClientHelper echoClient1 (interfaces.GetAddress (0), portNode0); // node1 targets node0
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get(1));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (simTime - 1.0));

  // Enable Flow Monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

  // Enable animation output
  AnimationInterface anim ("adhoc_echo_bidirectional.xml");

  // Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Flow Monitor statistics analysis
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::cout << "\n========== Flow Monitor Results ==========\n";
  for (auto const &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "Flow ID: " << flow.first << "   (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
      std::cout << "  Throughput: " 
                << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) ) / 1000 << " kbps\n";
      std::cout << "  Mean Delay: " 
                << (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) << " s\n";
      double pdr = (double) flow.second.rxPackets / flow.second.txPackets;
      std::cout << "  Packet Delivery Ratio: " << pdr * 100 << " %\n";
      std::cout << "-----------------------------------------\n";
    }

  Simulator::Destroy ();
  return 0;
}