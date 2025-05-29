#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/aodv-module.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHandover");

int main(int argc, char *argv[]) {
  bool enableRtsCts = false;
  std::string rate ("54Mbps");
  double simulationTime = 10;
  bool verbose = false;
  bool tracing = false;
  std::string phyMode ("DsssRate11Mbps");


  CommandLine cmd;
  cmd.AddValue ("EnableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue ("Rate", "Wifi data rate", rate);
  cmd.AddValue ("SimulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("Verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("Tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("phyMode", "Wifi PHY mode", phyMode);
  cmd.Parse (argc, argv);

  if (verbose) {
    LogComponentEnable ("WifiNetDevice", LOG_LEVEL_ALL);
    LogComponentEnable ("AdhocWifiMac", LOG_LEVEL_ALL);
  }

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (enableRtsCts ? "0" : "2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("MmWaveHalfRate"));

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(6);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  // disable fragmentation
  Config::SetDefault ("ns3::WifiMacQueue::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Set guard interval
  Config::SetDefault ("ns3::WifiRemoteStationManager::ShortGuardEnabled", BooleanValue (true) );

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices = wifi.Install (wifiPhy, wifiMac, apNodes);

  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (wifiPhy, wifiMac, staNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0 20 0 20"));
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);

  Ptr<Node> apNode0 = apNodes.Get (0);
  Ptr<ConstantPositionMobilityModel> ap0mobility = apNode0->GetObject<ConstantPositionMobilityModel> ();
  Vector3d ap0position (5.0, 5.0, 0.0);
  ap0mobility->SetPosition (ap0position);

  Ptr<Node> apNode1 = apNodes.Get (1);
  Ptr<ConstantPositionMobilityModel> ap1mobility = apNode1->GetObject<ConstantPositionMobilityModel> ();
  Vector3d ap1position (15.0, 15.0, 0.0);
  ap1mobility->SetPosition (ap1position);


  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  //Enable AODV routing
  AodvHelper aodv;
  // You can configure AODV attributes here using aodv.Set(...)

  // Install AODV protocol on all nodes
  // Note that you need to use the stack variable and not create new InternetStackHelper objects
  stack.SetRoutingHelper (aodv);


  if (tracing) {
    wifiPhy.EnablePcap ("wifi-handover", apDevices);
    wifiPhy.EnablePcap ("wifi-handover", staDevices);
  }

  AnimationInterface anim ("wifi-handover.xml");
  anim.SetConstantPosition (apNodes.Get (0), 5.0, 5.0);
  anim.SetConstantPosition (apNodes.Get (1), 15.0, 15.0);


  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }


  Simulator::Destroy ();

  return 0;
}