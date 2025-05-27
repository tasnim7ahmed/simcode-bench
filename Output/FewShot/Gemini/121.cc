#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/uinteger.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Disable fragmentation
  UintegerValue fragSize = 2200;
  Config::SetDefault ("ns3::WifiMacHeader::FragmentThreshold", fragSize);
  Config::SetDefault ("ns3::WifiMacHeader::RtsCtsThreshold", UintegerValue(0));

  // Create Nodes
  NodeContainer commandNode;
  commandNode.Create(1);

  NodeContainer medicalUnits;
  medicalUnits.Create(3);

  NodeContainer team1, team2, team3;
  team1.Create(5);
  team2.Create(5);
  team3.Create(5);

  // Combine all nodes
  NodeContainer allNodes;
  allNodes.Add(commandNode);
  allNodes.Add(medicalUnits);
  allNodes.Add(team1);
  allNodes.Add(team2);
  allNodes.Add(team3);

  // Set up WiFi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

  // Mobility Model
  MobilityHelper mobility;

  // Command and medical units are stationary
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(commandNode);
  mobility.Install(medicalUnits);

  // Teams have random positions and mobility
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));

  mobility.Install(team1);
  mobility.Install(team2);
  mobility.Install(team3);

  // Internet Stack
  InternetStackHelper internet;
  internet.Install(allNodes);

  // IP Addressing
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // UDP Echo Applications
  uint16_t portBase = 9;
  ApplicationContainer serverApps[3];
  ApplicationContainer clientApps[3];

  for (int i = 0; i < 3; ++i) {
    UdpEchoServerHelper echoServer(portBase + i);
    serverApps[i] = echoServer.Install(team1.Get(i)); // first 3 nodes of team 1 will be servers
    serverApps[i].Start(Seconds(1.0));
    serverApps[i].Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(i), portBase + i); // connect to team server
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));

    clientApps[i] = echoClient.Install(commandNode.Get(0));
    clientApps[i].Start(Seconds(2.0));
    clientApps[i].Stop(Seconds(10.0));
  }

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation
  AnimationInterface anim("disaster_recovery.xml");
  anim.SetConstantPosition(commandNode.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(medicalUnits.Get(0), 10.0, 0.0);
  anim.SetConstantPosition(medicalUnits.Get(1), 0.0, 10.0);
  anim.SetConstantPosition(medicalUnits.Get(2), 10.0, 10.0);

  // Run Simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Print Flow Monitor Statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << "\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
    std::cout << "  End to End Delay: " << i->second.delaySum << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}