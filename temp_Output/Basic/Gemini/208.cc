#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enable_netanim = true;
  double simulation_time = 10.0;
  std::string phyMode("DsssRate1Mbps");
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("EnableNetanim", "Enable Netanim visualization", enable_netanim);
  cmd.AddValue("SimulationTime", "Simulation time in seconds", simulation_time);
  cmd.AddValue("PhyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("Verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
    LogComponentEnable("YansWifiPhy", LOG_LEVEL_ALL);
    LogComponentEnable("YansWifiChannel", LOG_LEVEL_ALL);
    LogComponentEnable("AdhocWifiMac", LOG_LEVEL_ALL);
  }

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(6);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  WifiMacHelper mac;

  Ssid ssid1 = Ssid("ns3-ssid-1");
  Ssid ssid2 = Ssid("ns3-ssid-2");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1));

  NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2));

  NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));


  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes);

  NetDeviceContainer apDevices;
  apDevices.Add(apDevices1);
  apDevices.Add(apDevices2);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("0 50 0 50"));

  mobility.Install(staNodes);
  mobility.Install(apNodes);

  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
  address.NewNetwork();
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulation_time));

  UdpEchoClientHelper echoClient(apInterfaces1.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulation_time));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (enable_netanim) {
    AnimationInterface anim("wifi-handover.xml");
    anim.SetConstantPosition(apNodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(apNodes.Get(1), 40.0, 40.0);

  }

  Simulator::Stop(Seconds(simulation_time));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Flow active:  " << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << " s\n";
  }

  Simulator::Destroy();

  return 0;
}