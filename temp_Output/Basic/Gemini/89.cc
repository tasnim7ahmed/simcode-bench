#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("EnergyHarvestingSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (0));

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign (staDevices);
  interfaces = address.Assign (apDevices);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (12.0));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("InitialEnergyJoule", DoubleValue (1.0));

  EnergySourceContainer sources = basicSourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));

  radioEnergyModelHelper.Install (staDevices, sources);
  radioEnergyModelHelper.Install (apDevices, sources);

  BasicEnergyHarvesterHelper basicHarvesterHelper;
  basicHarvesterHelper.Set ("HarvestingUpdateInterval", TimeValue (Seconds (1.0)));

  EnergyHarvesterContainer harvesters = basicHarvesterHelper.Install (nodes);

  Ptr<UniformRandomVariable> randomVariate = CreateObject<UniformRandomVariable> ();
  randomVariate->SetAttribute ("Min", DoubleValue (0.0));
  randomVariate->SetAttribute ("Max", DoubleValue (0.1));

  for (uint32_t i = 0; i < harvesters.GetN (); ++i)
    {
      Ptr<BasicEnergyHarvester> harvester = DynamicCast<BasicEnergyHarvester> (harvesters.Get (i));
      harvester->SetRandomVariable (randomVariate);
    }

  // Tracing
  if (tracing)
    {
      phy.EnablePcap ("energy-harvesting", apDevices.Get (0));
    }

  // Output energy consumption
  Simulator::Schedule (Seconds (0.1), [&] () {
      NS_LOG_UNCOND ("--- Energy Tracing ---");
    });

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<EnergySource> energySource = sources.Get(i);

         Simulator::Schedule (Seconds (0.1), [&, node, energySource, i]() {
            std::stringstream ss;
            ss << "Node " << i << " Energy Source:" << std::endl;
            ss << "  Remaining energy: " << energySource->GetRemainingEnergy() << " J" << std::endl;
            NS_LOG_UNCOND (ss.str());
        });

        Simulator::Schedule (Seconds (12.0), [&, node, energySource, i]() {
            std::stringstream ss;
            ss << "Node " << i << " Energy Source:" << std::endl;
            ss << "  Remaining energy: " << energySource->GetRemainingEnergy() << " J" << std::endl;
            NS_LOG_UNCOND (ss.str());
        });
    }

  //FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (12.0));

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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}