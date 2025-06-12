#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

class EnergyLogger : public Object {
public:
  static TypeId GetTypeId (void);
  EnergyLogger(Ptr<BasicEnergySource> energySource, Ptr<DeviceEnergyModel> wifiEnergyModel);
  void Log();

private:
  Ptr<BasicEnergySource> m_energySource;
  Ptr<DeviceEnergyModel> m_wifiEnergyModel;
};

TypeId
EnergyLogger::GetTypeId (void)
{
  static TypeId tid = TypeId ("EnergyLogger")
    .SetParent<Object> ()
    .AddConstructor<EnergyLogger> ()
  ;
  return tid;
}

EnergyLogger::EnergyLogger(Ptr<BasicEnergySource> energySource, Ptr<DeviceEnergyModel> wifiEnergyModel)
  : m_energySource(energySource), m_wifiEnergyModel(wifiEnergyModel)
{
}

void
EnergyLogger::Log()
{
  double residualEnergy = m_energySource->GetRemainingEnergy();
  double totalEnergyConsumed = m_wifiEnergyModel->GetTotalEnergyConsumption();
  double harvestedPower = m_energySource->GetCurrentPower();
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s | Residual Energy: " << residualEnergy << " J | Consumed: " << totalEnergyConsumed << " J | Harvested Power: " << harvestedPower << " W");
}

void
ScheduleNextPacket(Ptr<Socket> socket, uint32_t packetSize, Address destAddress)
{
  if (Simulator::Now() < Seconds(10))
    {
      Simulator::Schedule(Seconds(1), &ScheduleNextPacket, socket, packetSize, destAddress);
      Ptr<Packet> packet = Create<Packet>(packetSize);
      socket->SendTo(packet, 0, destAddress);
    }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Energy configuration
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("InitialEnergy", DoubleValue(1.0));
  energySourceHelper.Set("SupplyVoltage", DoubleValue(3.7));

  BasicEnergyHarvesterHelper energyHarvesterHelper;
  energyHarvesterHelper.Set("HarvestedPower", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=0.1]"));

  EnergySourceContainer sources = energySourceHelper.Install(nodes);
  energyHarvesterHelper.Install(sources);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));

  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  // Setup application
  uint16_t port = 9;
  Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", localAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  InetSocketAddress remoteAddress(interfaces.GetAddress(1), port);
  remoteAddress.SetTtl(255);
  Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
  socket->Connect(remoteAddress);

  // Schedule packets every second for 10 seconds
  Simulator::Schedule(Seconds(1), &ScheduleNextPacket, socket, 1024, remoteAddress);

  // Logging setup
  Ptr<EnergyLogger> logger = CreateObject<EnergyLogger>(DynamicCast<BasicEnergySource>(sources.Get(1)), DynamicCast<DeviceEnergyModel>(deviceModels.Get(1)));
  Simulator::Schedule(Seconds(1), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(2), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(3), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(4), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(5), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(6), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(7), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(8), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(9), &EnergyLogger::Log, logger);
  Simulator::Schedule(Seconds(10), &EnergyLogger::Log, logger);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}