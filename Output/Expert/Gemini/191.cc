#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN-Energy");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t numNodes = 9;
  double simulationTime = 100;
  double nodeRange = 50.0;
  double packetInterval = 1.0;
  uint32_t packetSize = 1024;

  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of sensor nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("packetInterval", "Packet interval in seconds", packetInterval);
  cmd.AddValue ("packetSize", "Size of packet", packetSize);
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("WSN-Energy", LOG_LEVEL_ALL);
    }

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns-3-ssid")));
  NodeContainer nodes;
  nodes.Create (numNodes);

  NodeContainer sinkNode = NodeContainer (nodes.Get (0));
  NodeContainer sensorNodes;
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      sensorNodes.Add (nodes.Get (i));
    }

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiChannel.AddPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (nodeRange),
                                 "DeltaY", DoubleValue (nodeRange),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer sinkApp = server.Install (sinkNode);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  UdpClientHelper client (interfaces.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      clientApps.Add (client.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (100));

  EnergySourceContainer sources = basicSourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.00009));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.00054));

  DeviceEnergyModelContainer deviceModels = radioEnergyModelHelper.Install (devices, sources);

  Simulator::Schedule (Seconds (simulationTime), []() {
    std::cout << "Simulation finished" << std::endl;
  });

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();

  return 0;
}