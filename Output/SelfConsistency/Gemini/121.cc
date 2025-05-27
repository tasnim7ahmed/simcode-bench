#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numMedicalUnits = 3;
  uint32_t numResponderTeams = 3;
  uint32_t numNodesPerTeam = 5;
  double simulationTime = 20.0;
  std::string phyMode("DsssRate1Mbps");

  // Create nodes
  NodeContainer commandNode;
  commandNode.Create(1);

  NodeContainer medicalUnits;
  medicalUnits.Create(numMedicalUnits);

  NodeContainer responderTeams[numResponderTeams];
  for (uint32_t i = 0; i < numResponderTeams; ++i) {
    responderTeams[i].Create(numNodesPerTeam);
  }

  // Create all nodes container
  NodeContainer allNodes;
  allNodes.Add(commandNode);
  allNodes.Add(medicalUnits);
  for (uint32_t i = 0; i < numResponderTeams; ++i) {
    allNodes.Add(responderTeams[i]);
  }

  // Configure WiFi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Disable rate control
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  // Enable RTS/CTS
  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default();
  wifiMac.SetType(AD_HOC);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode",StringValue (phyMode),
                                  "ControlMode",StringValue (phyMode));

  NetDeviceContainer commandDevice = wifi.Install(wifiPhy, wifiMac, commandNode);
  NetDeviceContainer medicalUnitDevices = wifi.Install(wifiPhy, wifiMac, medicalUnits);

  NetDeviceContainer responderTeamDevices[numResponderTeams];
  for (uint32_t i = 0; i < numResponderTeams; ++i) {
    responderTeamDevices[i] = wifi.Install(wifiPhy, wifiMac, responderTeams[i]);
  }


  // Configure mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=10]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));

  for (uint32_t i = 0; i < numResponderTeams; ++i) {
    mobility.Install(responderTeams[i]);
  }

  // Make command node and medical units stationary
  MobilityHelper stationaryMobility;
  stationaryMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  stationaryMobility.Install(commandNode);
  stationaryMobility.Install(medicalUnits);

  // Set fixed positions for command node and medical units.
  Ptr<ConstantPositionMobilityModel> commandMobility = commandNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
  commandMobility->SetPosition(Vector(0.0, 0.0, 0.0));

  for (uint32_t i = 0; i < numMedicalUnits; ++i) {
    Ptr<ConstantPositionMobilityModel> medicalMobility = medicalUnits.Get(i)->GetObject<ConstantPositionMobilityModel>();
    medicalMobility->SetPosition(Vector((i + 1) * 10.0, 0.0, 0.0));
  }



  // Configure IP addresses
  InternetStackHelper internet;
  internet.Install(allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(allNodes);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configure UDP echo applications
  uint16_t port = 9;
  for (uint32_t i = 0; i < numResponderTeams; ++i) {
    UdpEchoServerHelper echoServer(port + i);
    ApplicationContainer serverApps = echoServer.Install(responderTeams[i].Get(0)); // install on the first node of each team
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(commandNode.Get(0)->GetId()), port + i);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(commandNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 2.0));
  }


  // Configure flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Configure animation
  AnimationInterface anim("disaster-recovery.xml");
  anim.SetMaxPktsPerTraceFile(10000000);

  // Run simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Analyze flow monitor data
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Packet Loss: " << (double)(i->second.txPackets - i->second.rxPackets) / i->second.txPackets * 100 << "%\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("disaster-recovery.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}