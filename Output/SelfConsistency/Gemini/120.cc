#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool enableRoutingTablePrinting = false;
  std::string phyMode ("DsssRate11Mbps");
  double simulationTime = 20; //seconds
  double routingTablePrintInterval = 5; //seconds

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableRoutingTablePrinting", "Enable routing table printing", enableRoutingTablePrinting);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("routingTablePrintInterval", "Routing table printing interval in seconds", routingTablePrintInterval);
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (AodvRoutingProtocol::TypeId(), LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (10);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelay");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
                                  "SystemLoss", DoubleValue (1),
                                  "Frequency", DoubleValue (2.4e9));
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.AddProtocol (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, 50, 50)));
  mobility.Install (nodes);

  // UDP Echo Server on each node
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1.0));

  // UDP Echo Client sending to the next node in a circular fashion
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    Ipv4Address destAddr = interfaces.GetAddress ((i + 1) % nodes.GetN (), 0);
    UdpEchoClientHelper echoClient (destAddr, 9);
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (4.0)));
    echoClient.SetAttribute ("MaxPackets", UintegerValue (5));

    ApplicationContainer clientApps = echoClient.Install (nodes.Get (i));
    clientApps.Start (Seconds (2.0 + i * 0.1));
    clientApps.Stop (Seconds (simulationTime - 2.0));
  }

  if (enablePcap)
  {
    wifiPhy.EnablePcapAll ("adhoc-aodv");
  }

  if (enableRoutingTablePrinting)
  {
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Simulator::Schedule (Seconds (routingTablePrintInterval), &Ipv4RoutingHelper::PrintRoutingTableAllAt, routingTablePrintInterval, nodes.Get (i));
    }
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable NetAnim
  AnimationInterface anim ("adhoc-aodv.anim");

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

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
      std::cout << "  Packet Delivery Ratio: " << double(i->second.rxPackets) / double(i->second.txPackets) << "\n";
      std::cout << "  End to End Delay: " << i->second.delaySum << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}