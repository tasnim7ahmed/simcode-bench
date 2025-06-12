#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergySimulation");

class EnergyTracer {
public:
  static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_INFO("Remaining energy: " << remainingEnergy << " J");
  }

  static void TotalEnergyConsumption(double totalEnergy) {
    NS_LOG_INFO("Total energy consumed: " << totalEnergy << " J");
  }
};

int main(int argc, char *argv[]) {
  uint32_t packetSize = 1024;
  double startTime = 1.0;
  double stopTime = 10.0;
  double distance = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of the packets in bytes", packetSize);
  cmd.AddValue("startTime", "Start time for the simulation (seconds)", startTime);
  cmd.AddValue("stopTime", "Stop time for the simulation (seconds)", stopTime);
  cmd.AddValue("distance", "Distance between nodes (meters)", distance);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(startTime));
  serverApp.Stop(Seconds(stopTime));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(startTime));
  clientApp.Stop(Seconds(stopTime));

  EnergySourceContainer sources;
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
  sources = basicSourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeCallback(&EnergyTracer::TotalEnergyConsumption));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/Tx", MakeCallback(&EnergyTracer::TotalEnergyConsumption));

  for (auto node : nodes) {
    Ptr<EnergySource> source = sources.Get(node->GetId());
    source->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&EnergyTracer::RemainingEnergy));
  }

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}