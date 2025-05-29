#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  uint32_t numNodes = 10;
  double simulationTime = 50; //seconds
  double dataRate = 1000; //bytes per second
  double packetInterval = 0.1;
  uint16_t port = 9;

  NodeContainer nodes;
  nodes.Create(numNodes);

  NodeContainer sinkNode;
  sinkNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(nodes);
  allNodes.Add(sinkNode);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(olsr);
  internet.Install(allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  // Sink Application
  UdpServerHelper server(port);
  ApplicationContainer sinkApp = server.Install(sinkNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime));

  // Client Application
  UdpClientHelper client(interfaces.GetAddress(numNodes), port);
  client.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(simulationTime / packetInterval)));
  client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i) {
    clientApps.Add(client.Install(nodes.Get(i)));
  }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.0174));
  radioEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.0197));
  radioEnergyModelHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
  radioEnergyModelHelper.Set("TransitionCurrentA", DoubleValue(0.0000015));
  DeviceEnergyModelContainer deviceModels = radioEnergyModelHelper.Install(devices, sources);

  // Simulation
  Simulator::Stop(Seconds(simulationTime));

  AnimationInterface anim("olsr-sensor-network.xml");
  anim.SetConstantPosition(sinkNode.Get(0), 20.0, 20.0);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}