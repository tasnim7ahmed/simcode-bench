#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyConsumptionExample");

class EnergyTracer {
public:
  static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_UNCOND("Remaining energy = " << remainingEnergy << " J");
  }

  static void TotalEnergyConsumption(double oldValue, double totalEnergy) {
    NS_LOG_UNCOND("Total energy consumed = " << totalEnergy << " J");
  }
};

int main(int argc, char *argv[]) {
  uint32_t packetSize = 1024;
  double startTime = 1.0;
  double distance = 10.0;
  double simulationTime = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of the UDP packet in bytes", packetSize);
  cmd.AddValue("startTime", "Start time for the simulation (seconds)", startTime);
  cmd.AddValue("distance", "Distance between nodes (meters)", distance);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Setup UDP application
  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(startTime));
  apps.Stop(Seconds(simulationTime));

  // Receiver application
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simulationTime));

  // Energy source setup
  BasicEnergySourceHelper basicEnergySourceHelper;
  basicEnergySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
  basicEnergySourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.7));

  EnergySourceContainer sources = basicEnergySourceHelper.Install(nodes);

  // Wi-Fi radio energy model helper
  WifiRadioEnergyModelHelper wifiEnergyHelper;
  wifiEnergyHelper.Set("TxCurrentA", DoubleValue(0.350)); // Transmission current in Amps
  wifiEnergyHelper.Set("RxCurrentA", DoubleValue(0.190)); // Reception current in Amps

  DeviceEnergyModelContainer deviceModels = wifiEnergyHelper.Install(devices, sources);

  // Tracing remaining energy and total energy consumption
  Config::Connect("/NodeList/*/DeviceEnergyModelList/0/RemainingEnergy",
                  MakeCallback(&EnergyTracer::RemainingEnergy));
  Config::Connect("/NodeList/*/DeviceEnergyModelList/0/TotalEnergyConsumption",
                  MakeCallback(&EnergyTracer::TotalEnergyConsumption));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}