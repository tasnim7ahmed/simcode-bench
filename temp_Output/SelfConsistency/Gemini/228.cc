#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sumo-mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSumoDsr");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);
  LogComponent::SetPrintFilter (std::string ("VanetSumoDsr"));

  // Create Nodes
  NodeContainer nodes;
  nodes.Create (20); // 20 vehicles

  // Install Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211p);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("vanet-network");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Install Mobility via SUMO
  SumoMobilityHelper sumo;
  sumo.sumoConfig ("osm.sumocfg"); // Replace with your SUMO config file
  sumo.Install (nodes);

  // Install DSR routing
  DsrHelper dsr;
  DsrMainRoutingHelper dsrRoutingHelper;
  Ipv4ListRoutingHelper list;
  list.Add (dsrRoutingHelper, 0);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Create Applications (UDP traffic)
  uint16_t port = 9; // Discard port (RFC 863)

  // Install receiver on node 10
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (10));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (79.0));

  // Install sender on node 0, sending to node 10
  UdpClientHelper client (interfaces.GetAddress (10), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // packets/second
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (78.0));

  // Tracing
  // Pcap tracing
  phy.EnablePcapAll("vanet-sumo-dsr");

  // Flowmonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation
  AnimationInterface anim ("vanet-sumo-dsr.xml");
  anim.EnablePacketMetadata ();

  // Run the simulation
  Simulator::Stop (Seconds (80));
  Simulator::Run ();

  //Flowmonitor analysis
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
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("vanet-sumo-dsr.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}