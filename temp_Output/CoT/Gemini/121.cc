#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
  bool enableRtsCts = true;
  std::string phyMode("DsssRate11Mbps");
  uint32_t numTeams = 3;
  uint32_t numNodesPerTeam = 5;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("numTeams", "Number of teams", numTeams);
  cmd.AddValue("numNodesPerTeam", "Number of nodes per team", numNodesPerTeam);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer commandNode;
  commandNode.Create(1);

  NodeContainer medicalUnits;
  medicalUnits.Create(3);

  NodeContainer teams[numTeams];
  for (uint32_t i = 0; i < numTeams; ++i) {
    teams[i].Create(numNodesPerTeam);
  }

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  if (enableRtsCts) {
      wifiMac.SetEnableRtsCts(true);
  }

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  NetDeviceContainer commandDevices = wifi.Install(wifiPhy, wifiMac, commandNode);
  NetDeviceContainer medicalDevices = wifi.Install(wifiPhy, wifiMac, medicalUnits);

  NetDeviceContainer teamDevices[numTeams];
  for (uint32_t i = 0; i < numTeams; ++i) {
    teamDevices[i] = wifi.Install(wifiPhy, wifiMac, teams[i]);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));

  for (uint32_t i = 0; i < numTeams; ++i) {
    mobility.Install(teams[i]);
  }

  MobilityHelper staticMobility;
  staticMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staticMobility.Install(commandNode);
  staticMobility.Install(medicalUnits);

  InternetStackHelper internet;
  internet.Install(commandNode);
  internet.Install(medicalUnits);
  for (uint32_t i = 0; i < numTeams; ++i) {
    internet.Install(teams[i]);
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer commandInterfaces = address.Assign(commandDevices);

  address.NewNetwork();
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer medicalInterfaces = address.Assign(medicalDevices);

  Ipv4InterfaceContainer teamInterfaces[numTeams];
  for (uint32_t i = 0; i < numTeams; ++i) {
    address.NewNetwork();
    address.SetBase(("10.1." + std::to_string(3 + i) + ".0").c_str(), "255.255.255.0");
    teamInterfaces[i] = address.Assign(teamDevices[i]);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  for (uint32_t i = 0; i < numTeams; ++i) {
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(teams[i].Get(0)); // Install on the first node of each team
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1.0));

    UdpEchoClientHelper echoClient(teamInterfaces[i].GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t j = 1; j < numNodesPerTeam; ++j) {
      ApplicationContainer clientApps = echoClient.Install(teams[i].Get(j));
      clientApps.Start(Seconds(2.0));
      clientApps.Stop(Seconds(simulationTime - 2.0));
    }
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("disaster-recovery-network.xml");
  anim.SetConstantPosition(commandNode.Get(0), 0, 20);
  anim.SetConstantPosition(medicalUnits.Get(0), 20, 20);
  anim.SetConstantPosition(medicalUnits.Get(1), 40, 20);
  anim.SetConstantPosition(medicalUnits.Get(2), 60, 20);

  Simulator::Stop(Seconds(simulationTime));

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Packet Loss: " << (double)(i->second.txPackets - i->second.rxPackets) / i->second.txPackets << "\n";
    std::cout << "  End to End Delay: " << i->second.delaySum << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}