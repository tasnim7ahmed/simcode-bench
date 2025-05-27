#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                     StringValue("0"));

  NodeContainer commandNode;
  commandNode.Create(1);

  NodeContainer medicalUnits;
  medicalUnits.Create(3);

  NodeContainer team1Nodes, team2Nodes, team3Nodes;
  team1Nodes.Create(5);
  team2Nodes.Create(5);
  team3Nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer commandDevice = wifi.Install(phy, mac, commandNode);
  NetDeviceContainer medicalDevices = wifi.Install(phy, mac, medicalUnits);
  NetDeviceContainer team1Devices = wifi.Install(phy, mac, team1Nodes);
  NetDeviceContainer team2Devices = wifi.Install(phy, mac, team2Nodes);
  NetDeviceContainer team3Devices = wifi.Install(phy, mac, team3Nodes);

  InternetStackHelper internet;
  internet.Install(commandNode);
  internet.Install(medicalUnits);
  internet.Install(team1Nodes);
  internet.Install(team2Nodes);
  internet.Install(team3Nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer commandInterfaces = address.Assign(commandDevice);
  address.NewNetwork();
  Ipv4InterfaceContainer medicalInterfaces = address.Assign(medicalDevices);
  address.NewNetwork();
  Ipv4InterfaceContainer team1Interfaces = address.Assign(team1Devices);
  address.NewNetwork();
  Ipv4InterfaceContainer team2Interfaces = address.Assign(team2Devices);
  address.NewNetwork();
  Ipv4InterfaceContainer team3Interfaces = address.Assign(team3Nodes);

  MobilityHelper commandMobility;
  commandMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                        "MinX", DoubleValue(0.0), "MinY",
                                        DoubleValue(0.0), "DeltaX",
                                        DoubleValue(5.0), "DeltaY",
                                        DoubleValue(5.0), "GridWidth",
                                        UintegerValue(1), "LayoutType",
                                        StringValue("RowFirst"));
  commandMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  commandMobility.Install(commandNode);

  MobilityHelper medicalMobility;
  medicalMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                        "MinX", DoubleValue(10.0), "MinY",
                                        DoubleValue(0.0), "DeltaX",
                                        DoubleValue(5.0), "DeltaY",
                                        DoubleValue(5.0), "GridWidth",
                                        UintegerValue(3), "LayoutType",
                                        StringValue("RowFirst"));
  medicalMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  medicalMobility.Install(medicalUnits);

  MobilityHelper team1Mobility, team2Mobility, team3Mobility;
  team1Mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                                 RectangleValue(Rectangle(0, 0, 50, 50)));
  team2Mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                                 RectangleValue(Rectangle(0, 0, 50, 50)));
  team3Mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                                 RectangleValue(Rectangle(0, 0, 50, 50)));

  team1Mobility.Install(team1Nodes);
  team2Mobility.Install(team2Nodes);
  team3Mobility.Install(team3Nodes);

  uint16_t port1 = 9, port2 = 10, port3 = 11;

  UdpEchoServerHelper echoServer1(port1);
  ApplicationContainer serverApps1 = echoServer1.Install(team1Nodes.Get(0));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(team1Interfaces.GetAddress(0), port1);
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(commandNode.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer2(port2);
  ApplicationContainer serverApps2 = echoServer2.Install(team2Nodes.Get(0));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(team2Interfaces.GetAddress(0), port2);
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(medicalUnits.Get(0));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer3(port3);
  ApplicationContainer serverApps3 = echoServer3.Install(team3Nodes.Get(0));
  serverApps3.Start(Seconds(1.0));
  serverApps3.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient3(team3Interfaces.GetAddress(0), port3);
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps3 = echoClient3.Install(medicalUnits.Get(1));
  clientApps3.Start(Seconds(2.0));
  clientApps3.Stop(Seconds(10.0));


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("disaster_recovery.xml");
  anim.SetConstantPosition(commandNode.Get(0),0,50);
  anim.SetConstantPosition(medicalUnits.Get(0),10,50);
  anim.SetConstantPosition(medicalUnits.Get(1),20,50);
  anim.SetConstantPosition(medicalUnits.Get(2),30,50);

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: "
              << double(i->second.rxPackets) / i->second.txPackets << "\n";
    std::cout << "  End-to-End Delay: " << i->second.delaySum << "\n";
    std::cout << "  Throughput: "
              << i->second.rxBytes * 8.0 /
                     (i->second.timeLastRxPacket.GetSeconds() -
                      i->second.timeFirstTxPacket.GetSeconds()) /
                     1024 / 1024
              << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}