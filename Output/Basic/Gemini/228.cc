#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sumo-mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
  bool enableSumo = true;
  std::string sumoScenario = "vanet.sumocfg";
  double simulationTime = 80.0;
  std::string phyMode("OfdmRate6Mbps");

  CommandLine cmd(__FILE__);
  cmd.AddValue("sumoScenario", "SUMO scenario file", sumoScenario);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("vanet-ssid")));
  NodeContainer nodes;
  nodes.Create(25);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(10));
  wifiPhy.Set("TxGain", DoubleValue(10));
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel");
  wifiChannel.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default();

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  DsrHelper dsr;
  DsrMainRoutingHelper dsrRoutingHelper;

  InternetStackHelper stack;
  stack.SetRoutingHelper(dsrRoutingHelper);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  if (enableSumo) {
    FileSumoMobilityHelper sumo;
    sumo.SumoBinary("sumo");
    sumo.TraceMobility(devices);
    sumo.ConfigFile(sumoScenario);
    sumo.Install(nodes);
  } else {
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
    mobility.Install(nodes);
  }

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(24));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(24), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 1));

  dsrRoutingHelper.AssignStreams(nodes, 1);

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowMonHelper;
  flowMonitor = flowMonHelper.InstallAll();

  Simulator::Stop(Seconds(simulationTime));

  AnimationInterface anim("vanet_animation.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(24), 50, 50);
  Simulator::Run();

  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
     }

  flowMonitor->SerializeToXmlFile("vanet_flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}