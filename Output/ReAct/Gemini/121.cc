#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer commandNode;
  commandNode.Create(1);

  NodeContainer medicalUnits;
  medicalUnits.Create(3);

  NodeContainer teamNodes[3];
  for (int i = 0; i < 3; ++i) {
    teamNodes[i].Create(5);
  }

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer commandDevices = wifi.Install(wifiPhy, wifiMac, commandNode);
  NetDeviceContainer medicalDevices = wifi.Install(wifiPhy, wifiMac, medicalUnits);

  NetDeviceContainer teamDevices[3];
  for (int i = 0; i < 3; ++i) {
    teamDevices[i] = wifi.Install(wifiPhy, wifiMac, teamNodes[i]);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("1s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "Bounds", StringValue("0 100 0 100"));

  for (int i = 0; i < 3; ++i) {
      mobility.Install(teamNodes[i]);
  }

  MobilityHelper commandMobility;
  commandMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  commandMobility.Install(commandNode);

  MobilityHelper medicalMobility;
  medicalMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  medicalMobility.Install(medicalUnits);

  InternetStackHelper internet;
  internet.Install(commandNode);
  internet.Install(medicalUnits);
  for (int i = 0; i < 3; ++i) {
    internet.Install(teamNodes[i]);
  }

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer commandInterfaces = ipv4.Assign(commandDevices);
  ipv4.NewNetwork();

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer medicalInterfaces = ipv4.Assign(medicalDevices);
  ipv4.NewNetwork();

  Ipv4InterfaceContainer teamInterfaces[3];
  for (int i = 0; i < 3; ++i) {
    ipv4.SetBase("10.1." + std::to_string(i + 3) + ".0", "255.255.255.0");
    teamInterfaces[i] = ipv4.Assign(teamDevices[i]);
    ipv4.NewNetwork();
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps[3];
  for (int i = 0; i < 3; ++i) {
      serverApps[i] = echoServer.Install(teamNodes[i].Get(0));
      serverApps[i].Start(Seconds(1.0));
      serverApps[i].Stop(Seconds(10.0));
  }

  UdpEchoClientHelper echoClient("10.1.3.1", 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[3];
  for (int i = 0; i < 3; ++i) {
      clientApps[i] = echoClient.Install(commandNode.Get(0));
      clientApps[i].Start(Seconds(2.0));
      clientApps[i].Stop(Seconds(10.0));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("disaster-recovery.xml");
  anim.SetConstantPosition(commandNode.Get(0), 50.0, 50.0);
  for(uint32_t i = 0; i < medicalUnits.GetN(); ++i){
        anim.SetConstantPosition(medicalUnits.Get(i), 75.0 + i * 5, 50.0);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
	Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio:   " << (double)i->second.rxBytes / (double)i->second.txBytes << "\n";
    std::cout << "  Delay Sum:   " << i->second.delaySum << "\n";
    std::cout << "  Mean End-to-End Delay:   " << i->second.delaySum / i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";

  }

  Simulator::Destroy();
  return 0;
}