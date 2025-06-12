#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/yans-wifi-phy.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSpectrumSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  double simulationTime = 10.0;
  double distance = 10.0;
  std::string wifiStandard = "802.11ax";
  std::string phyType = "SpectrumWifiPhy";
  std::string errorModelType = "NistErrorRateModel";
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds.", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes in meters.", distance);
  cmd.AddValue ("wifiStandard", "Wifi standard (e.g., 802.11ax).", wifiStandard);
  cmd.AddValue ("phyType", "Phy type (SpectrumWifiPhy or YansWifiPhy).", phyType);
  cmd.AddValue ("errorModelType", "Error Model Type", errorModelType);
  cmd.AddValue ("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.Parse (argc, argv);

  if (verbose) {
    LogComponentEnable ("WifiSpectrumSimulation", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (wifiStandard);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phyHelper;

  if (phyType == "SpectrumWifiPhy") {
    SpectrumWifiHelper spectrumWifiHelper;
    spectrumWifiHelper.SetStandard (wifiStandard);
    channelHelper = YansWifiChannelHelper::Default ();
    phyHelper = YansWifiPhyHelper::Default ();
    phyHelper.SetChannel (channelHelper.Create ());
  } else {
    channelHelper = YansWifiChannelHelper::Default ();
    phyHelper.SetChannel (channelHelper.Create ());
  }

  WifiMacHelper macHelper;
  Ssid ssid = Ssid ("ns-3-ssid");
  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifiHelper.Install (phyHelper, macHelper, nodes.Get (0));

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid),
                     "BeaconGeneration", BooleanValue (true),
                     "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices = wifiHelper.Install (phyHelper, macHelper, nodes.Get (1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer::Create (staDevices, apDevices));

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client (interfaces.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  client.SetAttribute ("PacketSize", UintegerValue (1472));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (simulationTime + 1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (pcapTracing)
    {
      phyHelper.EnablePcap ("wifi-spectrum-simulation", apDevices);
    }

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == interfaces.GetAddress (1))
        {
          std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000 / 1000  << " Mbps\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}