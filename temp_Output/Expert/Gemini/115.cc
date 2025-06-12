#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]) {

  bool enableFlowMonitor = true;
  bool enableNetAnim = true;
  std::string phyMode = "DsssRate1Mbps";
  double simulationTime = 10.0;
  double distance = 10.0;

  CommandLine cmd;
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("enableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.AddValue("enableNetAnim", "Enable network animation", enableNetAnim);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::Ssid", StringValue("adhoc-network"));
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                               StringValue(phyMode), "ControlMode",
                               StringValue(phyMode));
  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");
  NodeContainer nodes;
  nodes.Create(2);
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(i.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
  if (enableFlowMonitor) {
    monitor = flowmon.InstallAll();
  }

  if (enableNetAnim) {
    AnimationInterface anim("adhoc.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 40, 40);
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  if (enableFlowMonitor) {
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
      std::cout << "Flow ID: " << it->first << " Src Addr "
                << t.sourceAddress << " Dst Addr " << t.destinationAddress
                << " Src Port " << t.sourcePort << " Dst Port "
                << t.destinationPort << "\n";
      std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << it->second.lostPackets << "\n";
      std::cout << "  Packet Delivery Ratio: "
                << double(it->second.rxPackets) / it->second.txPackets << "\n";
      std::cout << "  Delay Sum: " << it->second.delaySum << "\n";
      std::cout << "  Mean Delay: " << it->second.delaySum / it->second.rxPackets << "\n";
      std::cout << "  Throughput: "
                << it->second.rxBytes * 8.0 /
                       (it->second.timeLastRxPacket.GetSeconds() -
                        it->second.timeFirstTxPacket.GetSeconds()) /
                       1024 / 1024
                << " Mbps\n";
    }
    monitor->SerializeToXmlFile("adhoc.flowmon", true, true);
  }

  Simulator::Destroy();
  return 0;
}