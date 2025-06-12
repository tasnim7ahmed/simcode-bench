#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyExample");

class EnergyTracer : public Object
{
public:
  static TypeId GetTypeId();
  void TraceRemainingEnergy(Ptr<const DoubleValue> value);
  void TraceTotalEnergyConsumption(Ptr<const DoubleValue> value);
};

TypeId
EnergyTracer::GetTypeId()
{
  static TypeId tid = TypeId("EnergyTracer").SetParent<Object>().AddConstructor<EnergyTracer>();
  return tid;
}

void
EnergyTracer::TraceRemainingEnergy(Ptr<const DoubleValue> value)
{
  double energy = (*value->GetPointer());
  NS_LOG_UNCOND(Simulator::Now().As(Time::S) << " Remaining Energy: " << energy << " J");
}

void
EnergyTracer::TraceTotalEnergyConsumption(Ptr<const DoubleValue> value)
{
  double energy = (*value->GetPointer());
  NS_LOG_UNCOND(Simulator::Now().As(Time::S) << " Total Energy Consumed: " << energy << " J");
}

int
main(int argc, char* argv[])
{
  uint32_t packetSize = 1024;      // bytes
  double simulationStartTime = 1.0; // seconds
  double simulationStopTime = 10.0;
  double nodeDistance = 10.0;      // meters

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
  cmd.AddValue("simulationStartTime", "Start time for the simulation (seconds)", simulationStartTime);
  cmd.AddValue("nodeDistance", "Distance between nodes (meters)", nodeDistance);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(nodeDistance),
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
  serverApps.Stop(Seconds(simulationStopTime));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(simulationStartTime));
  clientApps.Stop(Seconds(simulationStopTime));

  // Energy configuration
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0)); // 100 Joules initial energy

  WifiRadioEnergyModelHelper radioEnergyHelper;
  energySourceHelper.SetDeviceEnergyModelCreator("ns3::WifiNetDevice",
                                                "ns3::WifiRadioEnergyModel",
                                                "TxCurrentA", DoubleValue(0.350),
                                                "RxCurrentA", DoubleValue(0.350),
                                                "IdleCurrentA", DoubleValue(0.050),
                                                "SleepCurrentA", DoubleValue(0.010));

  EnergySourceContainer energySources = energySourceHelper.Install(nodes);

  // Tracing energy consumption
  EnergyTracer tracer;

  Config::Connect("/NodeList/0/deviceEnergyModelList/0/$ns3::WifiRadioEnergyModel/RemainingEnergy",
                  MakeCallback(&EnergyTracer::TraceRemainingEnergy, &tracer));
  Config::Connect("/NodeList/0/deviceEnergyModelList/0/$ns3::WifiRadioEnergyModel/TotalEnergyConsumption",
                  MakeCallback(&EnergyTracer::TraceTotalEnergyConsumption, &tracer));

  Simulator::Stop(Seconds(simulationStopTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}