#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/yans-wifi-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN_Energy");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  double simulationTime = 100.0;
  uint32_t numNodes = 25; // Number of sensor nodes
  double gridSide = 5.0; // Side length of the grid in meters
  double packetInterval = 1.0; // Seconds
  uint32_t packetSize = 1024; // Bytes
  uint32_t dataRate = 1024000; // bps

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("numNodes", "Number of sensor nodes", numNodes);
  cmd.AddValue ("gridSide", "Side length of the grid in meters", gridSide);
  cmd.AddValue ("packetInterval", "Packet interval in seconds", packetInterval);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "Data Rate in bps", dataRate);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("WSN_Energy", LOG_LEVEL_INFO);

  NodeContainer sensorNodes;
  sensorNodes.Create (numNodes);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer sensorDevices = wifi.Install (wifiPhy, wifiMac, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install (wifiPhy, wifiMac, sinkNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (gridSide),
                                 "DeltaY", DoubleValue (gridSide),
                                 "GridWidth", UintegerValue ((uint32_t) sqrt(numNodes)),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sinkNode);
  sinkNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(gridSide * sqrt(numNodes), gridSide * sqrt(numNodes), 0.0));

  InternetStackHelper internet;
  internet.Install (sensorNodes);
  internet.Install (sinkNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign (sensorDevices);
  Ipv4InterfaceContainer sinkInterface = ipv4.Assign (sinkDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;  // well-known echo port number

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (sinkInterface.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate (dataRate));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      clientApps.Add (onoff.Install (sensorNodes.Get (i)));
      clientApps.Get (i)->SetStartTime (Seconds (i * packetInterval));
      clientApps.Get (i)->SetStopTime (Seconds (simulationTime));
    }

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (sinkNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime));

  EnergySourceContainer sources;
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (100)); // Initial energy in Joules

  EnergySourceContainer energySources = basicSourceHelper.Install (sensorNodes);
  for (uint32_t i = 0; i < energySources.GetN(); ++i)
  {
      Ptr<BasicEnergySource> basicSourcePtr = energySources.Get(i)->GetObject<BasicEnergySource>();
      basicSourcePtr->SetNode (sensorNodes.Get(i)); // Ensure the energy source is linked to the node
  }

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.02)); // Transmit current in Amperes
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.01)); // Receive current in Amperes
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.00001)); // Sleep current in Amperes
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.001)); // Idle current in Amperes

  DeviceEnergyModelContainer deviceModels = radioEnergyModelHelper.Install (sensorDevices);
  for (uint32_t i = 0; i < sensorDevices.GetN(); ++i)
  {
      Ptr<DeviceEnergyModel> deviceEnergyModelPtr = deviceModels.Get(i)->GetObject<DeviceEnergyModel>();
      Ptr<BasicEnergySource> basicSourcePtr = energySources.Get(i)->GetObject<BasicEnergySource>();
      deviceEnergyModelPtr->SetEnergySource (basicSourcePtr);
  }

  Simulator::Schedule (Seconds (1.0), [&] () {
      for (uint32_t i = 0; i < energySources.GetN(); ++i) {
        Ptr<BasicEnergySource> basicSourcePtr = energySources.Get(i)->GetObject<BasicEnergySource>();
          NS_LOG_INFO ("Node " << i << " energy remaining: " << basicSourcePtr->GetRemainingEnergy() << " J");
          if (basicSourcePtr->GetRemainingEnergy() <= 0)
          {
              NS_LOG_INFO ("Node " << i << " has depleted its energy. Stopping simulation.");
              Simulator::Stop (Seconds(simulationTime));
              return; // Immediately exit, preventing further iterations
          }
      }
  });

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("wsn-energy");
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
      NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
      NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
      NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
      NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps");
    }

  Simulator::Destroy ();
  return 0;
}