#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/on-off-application.h"
#include "ns3/command-line.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiEnergy");

bool isAwake = true;

void CheckEnergy(Ptr<Node> node) {
    Ptr<EnergySource> source = node->GetObject<EnergySource>();
    if (source) {
        double currentEnergy = source->GetRemainingEnergy();
        if (currentEnergy <= 0 && isAwake) {
            isAwake = false;
            NS_LOG_UNCOND ("Node " << node->GetId() << " entering sleep state");
        }
    }
    Simulator::Schedule (Seconds(0.1), &CheckEnergy, node);
}

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string dataRate = "1000kbps";
  uint32_t packetSize = 1024;
  double txPowerLevel = 10.0;
  double initialEnergy = 100.0;
  double txCurrentA = 0.1;
  double rxCurrentA = 0.05;
  double idleCurrentA = 0.001;
  double sleepCurrentA = 0.0001;
  double voltage = 3.3;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("dataRate", "Data rate for the OnOff application", dataRate);
  cmd.AddValue ("packetSize", "Size of packets for the OnOff application", packetSize);
  cmd.AddValue ("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
  cmd.AddValue ("initialEnergy", "Initial energy (Joules)", initialEnergy);
  cmd.AddValue ("txCurrentA", "Tx current (A)", txCurrentA);
  cmd.AddValue ("rxCurrentA", "Rx current (A)", rxCurrentA);
  cmd.AddValue ("idleCurrentA", "Idle current (A)", idleCurrentA);
  cmd.AddValue ("sleepCurrentA", "Sleep current (A)", sleepCurrentA);
  cmd.AddValue ("voltage", "Voltage (V)", voltage);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces.GetAddress (1), 9)));
  onoff.SetConstantRate (DataRate (dataRate));
  onoff.SetPacketSize (packetSize);

  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper sink (interfaces.GetAddress (1), 9);
  sink.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  sink.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  sink.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
    sinkApps.Start (Seconds (1.0));
    sinkApps.Stop (Seconds (10.0));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("InitialEnergyJ", DoubleValue (initialEnergy));
  EnergySourceContainer sources = basicSourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (txCurrentA));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (rxCurrentA));
  radioEnergyHelper.Set ("IdleCurrentA", DoubleValue (idleCurrentA));
  radioEnergyHelper.Set ("SleepCurrentA", DoubleValue (sleepCurrentA));
  radioEnergyHelper.Set ("VoltageV", DoubleValue (voltage));
  radioEnergyHelper.Install (devices, sources);

  // Configure transmission power
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice> (devices.Get (i));
      Ptr<YansWifiPhy> phy = DynamicCast<YansWifiPhy> (wifiNetDevice->GetPhy ());
      phy->SetTxPowerStart (txPowerLevel);
      phy->SetTxPowerEnd (txPowerLevel);
    }

  // Tracing
  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("adhoc");
    }

  Simulator::Schedule (Seconds(0.1), &CheckEnergy, nodes.Get(0));
  Simulator::Schedule (Seconds(0.1), &CheckEnergy, nodes.Get(1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (10.0));

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
  	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000  << " kbps\n";
  	}
  Simulator::Destroy ();
  return 0;
}