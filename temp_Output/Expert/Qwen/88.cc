#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergySimulation");

class EnergyTracer {
public:
  static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_INFO("Node energy remaining: " << remainingEnergy << " J");
  }

  static void TotalEnergyConsumption(double oldValue, double totalEnergy) {
    NS_LOG_INFO("Total energy consumed: " << totalEnergy << " J");
  }
};

int main(int argc, char *argv[]) {
  uint32_t packetSize = 1024;
  double startTime = 1.0;
  double distance = 10.0;
  double simulationTime = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
  cmd.AddValue("startTime", "Start time for the simulation (seconds)", startTime);
  cmd.AddValue("distance", "Distance between nodes (meters)", distance);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

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
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(startTime));
  clientApps.Stop(Seconds(simulationTime));

  BasicEnergySourceHelper energySource;
  energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
  EnergyModelContainer wifiEnergyModel = wifi.GetPhy(0)->GetDeviceEnergyModel();
  EnergySourceContainer sources = energySource.Install(nodes);

  for (auto node : nodes) {
    Ptr<EnergySource> source = sources.Get(0);
    Config::Connect("/NodeList/*/$ns3::EnergySource/RenewableEnergyChange",
                    MakeCallback(&EnergyTracer::RemainingEnergy));
    Config::Connect("/NodeList/*/$ns3::EnergySource/TotalEnergyConsumption",
                    MakeCallback(&EnergyTracer::TotalEnergyConsumption));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}