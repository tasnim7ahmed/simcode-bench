#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DsdvVanetSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string phyMode ("DsssRate1Mbps");
  uint32_t numNodes = 10;
  double simulationTime = 10.0;
  double distance = 50; // Distance between nodes
  double speed = 10; // m/s

  CommandLine cmd (__FILE__);
  cmd.AddValue ("pcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  LogComponent::SetLogLevel (DsdvHelper::GetTypeId (), LOG_LEVEL_ALL);
  LogComponent::SetLogLevel (UdpClient::GetTypeId (), LOG_LEVEL_INFO);
  LogComponent::SetLogLevel (UdpServer::GetTypeId (), LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelay");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "Ssid", SsidValue (ssid));

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode",StringValue (phyMode),
                                  "ControlMode",StringValue (phyMode));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

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

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (speed, 0, 0));
    }

  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (dsdv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (numNodes - 1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simulationTime));

  UdpClientHelper client (interfaces.GetAddress (numNodes - 1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("dsdv-vanet");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";

    }

  Simulator::Destroy ();
  return 0;
}