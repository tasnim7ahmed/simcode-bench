#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {

  bool enableRtsCts = true;
  std::string phyMode("DsssRate1Mbps");
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds(2.0);
  uint32_t maxPackets = 100;

  CommandLine cmd;
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("phyMode", "Wifi PHY mode", phyMode);
  cmd.AddValue("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue("interPacketInterval", "Interval between packets", interPacketInterval);
  cmd.AddValue("maxPackets", "Max Packets to send", maxPackets);
  cmd.Parse(argc, argv);

  NodeContainer commandNode;
  commandNode.Create(1);

  NodeContainer medicalUnits;
  medicalUnits.Create(3);

  NodeContainer team1Nodes;
  team1Nodes.Create(5);

  NodeContainer team2Nodes;
  team2Nodes.Create(5);

  NodeContainer team3Nodes;
  team3Nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(-10));
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("Model", "ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("Model", "ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  if (enableRtsCts) {
    wifiMac.SetType("ns3::AdhocWifiMac", "RtsCtsThreshold", UintegerValue(0));
  } else {
    wifiMac.SetType("ns3::AdhocWifiMac");
  }

  NetDeviceContainer commandDevice = wifi.Install(wifiPhy, wifiMac, commandNode);
  NetDeviceContainer medicalDevices = wifi.Install(wifiPhy, wifiMac, medicalUnits);
  NetDeviceContainer team1Devices = wifi.Install(wifiPhy, wifiMac, team1Nodes);
  NetDeviceContainer team2Devices = wifi.Install(wifiPhy, wifiMac, team2Nodes);
  NetDeviceContainer team3Devices = wifi.Install(wifiPhy, wifiMac, team3Nodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator", "X", StringValue("0.0"),
                                "Y", StringValue("0.0"), "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(commandNode);
  mobility.Install(medicalUnits);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("1s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "Bounds", StringValue("0 10 0 10"));
  mobility.Install(team1Nodes);
  mobility.Install(team2Nodes);
  mobility.Install(team3Nodes);

  InternetStackHelper internet;
  internet.Install(commandNode);
  internet.Install(medicalUnits);
  internet.Install(team1Nodes);
  internet.Install(team2Nodes);
  internet.Install(team3Nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer commandInterfaces = ipv4.Assign(commandDevice);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer medicalInterfaces = ipv4.Assign(medicalDevices);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer team1Interfaces = ipv4.Assign(team1Devices);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer team2Interfaces = ipv4.Assign(team2Devices);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer team3Interfaces = ipv4.Assign(team3Devices);

  uint16_t port = 9;

  UdpEchoServerHelper echoServer1(port);
  ApplicationContainer serverApps1 = echoServer1.Install(team1Nodes.Get(0));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer2(port);
  ApplicationContainer serverApps2 = echoServer2.Install(team2Nodes.Get(0));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer3(port);
  ApplicationContainer serverApps3 = echoServer3.Install(team3Nodes.Get(0));
  serverApps3.Start(Seconds(1.0));
  serverApps3.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(team1Interfaces.GetAddress(0), port);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  echoClient1.SetAttribute("Interval", TimeValue(interPacketInterval));
  echoClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps1 = echoClient1.Install(team1Nodes.Get(1));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(team2Interfaces.GetAddress(0), port);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  echoClient2.SetAttribute("Interval", TimeValue(interPacketInterval));
  echoClient2.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps2 = echoClient2.Install(team2Nodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient3(team3Interfaces.GetAddress(0), port);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  echoClient3.SetAttribute("Interval", TimeValue(interPacketInterval));
  echoClient3.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps3 = echoClient3.Install(team3Nodes.Get(1));
  clientApps3.Start(Seconds(2.0));
  clientApps3.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("disaster-recovery-network.xml");

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}