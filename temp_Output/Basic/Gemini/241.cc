#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nVehicles = 1;
  double simulationTime = 10;
  double packetInterval = 1.0;
  uint16_t packetSize = 1024;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("numVehicles", "Number of vehicles", nVehicles);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("packetInterval", "Packet interval in seconds", packetInterval);
  cmd.AddValue ("packetSize", "Size of packet in bytes", packetSize);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("VanetSimulation", LOG_LEVEL_INFO);
    }

  NodeContainer vehicles;
  vehicles.Create (nVehicles);
  NodeContainer rsu;
  rsu.Create (1);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (nVehicles),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  Ptr<ConstantVelocityMobilityModel> vehicleMobility = vehicles.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  vehicleMobility->SetVelocity (Vector (10, 0, 0)); // Move vehicle along X axis

  MobilityHelper rsuMobility;
  rsuMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  rsuMobility.Install (rsu);
  rsu.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (50, 0, 0)); // RSU position

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  QosWaveMacHelper waveMac = QosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p;

  NetDeviceContainer vehicleDevices = wifi80211p.Install (wifiPhy, waveMac, vehicles);
  NetDeviceContainer rsuDevices = wifi80211p.Install (wifiPhy, waveMac, rsu);

  InternetStackHelper internet;
  internet.Install (vehicles);
  internet.Install (rsu);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign (vehicleDevices);
  Ipv4InterfaceContainer rsuInterfaces = ipv4.Assign (rsuDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpClientHelper client (rsuInterfaces.GetAddress (0), port);
  client.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
  client.SetAttribute ("MaxPackets", UintegerValue (static_cast<uint32_t>(simulationTime/packetInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (vehicles.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (rsu.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();

  return 0;
}