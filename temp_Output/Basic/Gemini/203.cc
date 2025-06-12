#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {

  bool enable_pcap = false;
  bool verbose = false;
  double simulation_time = 10; //seconds
  std::string phyMode("OfdmRate6MbpsBW10MHz");
  uint32_t packetSize = 1024; //bytes
  uint32_t numPackets = 1000;
  double interval = 0.01; //seconds
  std::string animFile = "vanet.xml";

  CommandLine cmd;
  cmd.AddValue("EnablePcap", "Capture all traffic to pcap file", enable_pcap);
  cmd.AddValue("Verbose", "Enable log components", verbose);
  cmd.AddValue("SimulationTime", "Simulation time in seconds", simulation_time);
  cmd.AddValue("PacketSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue("NumPackets", "Number of packets generated", numPackets);
  cmd.AddValue("Interval", "Interval between packets", interval);
  cmd.AddValue("AnimFile", "File name for animation output", animFile);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("ns-3-ssid")));

  NodeContainer nodes;
  nodes.Create(5);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(20.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(5),
                                 "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  // Set initial velocities
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    Vector velocity(10 + i * 2, 0, 0); // Varying speeds
    mob->SetVelocity(velocity);
  }

  // Wifi setup
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("TxPowerStart", DoubleValue(23));
  wifiPhy.Set("TxPowerEnd", DoubleValue(23));
  wifiPhy.Set("ChannelNumber", UintegerValue(180));
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // UDP server on node 0
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulation_time));

  // UDP client on other nodes
  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(numPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    clientApps.Add(client.Install(nodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulation_time));

  // Create pcap files if enabled
  if (enable_pcap) {
    wifiPhy.EnablePcapAll("vanet");
  }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation
  AnimationInterface anim(animFile);
  anim.EnablePacketMetadata(true);

  // Run simulation
  Simulator::Stop(Seconds(simulation_time));
  Simulator::Run();

  // Print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Packet Loss Ratio: " << (double)(i->second.txPackets - i->second.rxPackets)/(double)i->second.txPackets << "\n";
    std::cout << "  End To End Delay: " << i->second.delaySum << "\n";
  }

  Simulator::Destroy();
  return 0;
}