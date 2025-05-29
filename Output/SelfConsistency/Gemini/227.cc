#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UAVSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  double simulationTime = 60.0;
  uint32_t numUAVs = 5;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("numUAVs", "Number of UAVs", numUAVs);
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (UAVSimulation::LOG_PREFIX, LOG_LEVEL_INFO);

  NodeContainer uavNodes;
  uavNodes.Create (numUAVs);

  NodeContainer gcsNode;
  gcsNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (uavNodes);
  allNodes.Add (gcsNode);

  // Configure mobility for UAVs
  MobilityHelper mobilityUAV;
  mobilityUAV.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Z", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=50.0]"));
  mobilityUAV.SetMobilityModel ("ns3::RandomWalk3dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue ("1s"),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobilityUAV.Install (uavNodes);

  // Configure mobility for GCS (static)
  MobilityHelper mobilityGCS;
  mobilityGCS.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityGCS.Install (gcsNode);
  Ptr<Node> gcs = gcsNode.Get (0);
  Ptr<ConstantPositionMobilityModel> gcsMobility = gcs->GetObject<ConstantPositionMobilityModel> ();
  gcsMobility->SetPosition (Vector (50.0, 50.0, 0.0));

  // Configure Wifi
  WifiHelper wifi;
  WifiMacHelper wifiMac;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer uavDevices = wifi.Install (wifiPhy, wifiMac, uavNodes);
  NetDeviceContainer gcsDevices = wifi.Install (wifiPhy, wifiMac, gcsNode);

  NetDeviceContainer allDevices;
  allDevices.Add (uavDevices);
  allDevices.Add (gcsDevices);


  // Install AODV routing
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer uavInterfaces = address.Assign (uavDevices);
  Ipv4InterfaceContainer gcsInterfaces = address.Assign (gcsDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure UDP application (UAVs sending to GCS)
  uint16_t port = 9;
  UdpClientHelper client (gcsInterfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numUAVs; ++i)
    {
      clientApps.Add (client.Install (uavNodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime - 1));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (gcsNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime));

  // Enable PCAP tracing
  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("uav_simulation");
    }

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print Flow Monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
	  std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.Seconds () - i->second.timeFirstTxPacket.Seconds ()) / 1024  << " kbps\n";
    }


  Simulator::Destroy ();
  return 0;
}