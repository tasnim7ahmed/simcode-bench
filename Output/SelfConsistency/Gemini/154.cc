#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WirelessSimulation");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Set simulation time
  double simulationTime = 10.0;

  // Create nodes
  NodeContainer nodes;
  nodes.Create (5);

  // Configure wireless channel
  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Configure MAC layer
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "SgId", UintegerValue (0));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (0));
  staDevices.Add(wifi.Install (phy, mac, nodes.Get (1)));
  staDevices.Add(wifi.Install (phy, mac, nodes.Get (2)));
  staDevices.Add(wifi.Install (phy, mac, nodes.Get (3)));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "SgId", UintegerValue (0));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (4));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer::Create(staDevices, apDevices));

  // Create UDP server on node 4
  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  // Create UDP clients on nodes 0-3
  UdpClientHelper client0 (interfaces.GetAddress (4), 9);
  client0.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client0.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client0.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpClientHelper client1 (interfaces.GetAddress (4), 9);
  client1.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client1.SetAttribute ("PacketSize", UintegerValue (1024));

    UdpClientHelper client2 (interfaces.GetAddress (4), 9);
  client2.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));

    UdpClientHelper client3 (interfaces.GetAddress (4), 9);
  client3.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client3.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client3.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps0 = client0.Install (nodes.Get (0));
  clientApps0.Start (Seconds (2.0));
  clientApps0.Stop (Seconds (simulationTime + 1));

  ApplicationContainer clientApps1 = client1.Install (nodes.Get (1));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (simulationTime + 1));

    ApplicationContainer clientApps2 = client2.Install (nodes.Get (2));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (simulationTime + 1));

    ApplicationContainer clientApps3 = client3.Install (nodes.Get (3));
  clientApps3.Start (Seconds (2.0));
  clientApps3.Stop (Seconds (simulationTime + 1));


  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Create FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Analyze results
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  // Write data to CSV file
  std::ofstream outputFile;
  outputFile.open ("simulation_data.csv");
  outputFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    outputFile << t.sourceAddress << "," << t.destinationAddress << "," << i->second.txBytes << "," << i->second.timeFirstTxPacket.GetSeconds() << "," << i->second.timeLastRxPacket.GetSeconds() << std::endl;
  }

  outputFile.close ();

  Simulator::Destroy ();
  return 0;
}