#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

void
LogStats(Ptr<EnergySource> energySource, Ptr<BasicEnergyHarvester> harvester)
{
  double time = Simulator::Now().GetSeconds();
  double residualEnergy = energySource->GetRemainingEnergy();
  double totalEnergyConsumed = energySource->GetInitialEnergy() - residualEnergy;
  double lastHarvestedPower = harvester->GetHarvestedPower();
  std::cout << "Time: " << time
            << "s Residual Energy: " << residualEnergy
            << "J Total Consumed: " << totalEnergyConsumed
            << "J Last Harvested Power: " << lastHarvestedPower
            << "W" << std::endl;
  Simulator::Schedule(Seconds(1.0), &LogStats, energySource, harvester);
}

int
main(int argc, char *argv[])
{
  uint32_t numNodes = 2;
  double simulationTime = 10.0;
  double pktInterval = 1.0;
  double pktDuration = 0.0023;
  double initialEnergy = 1.0;
  double txCurrentA = 0.0174;
  double rxCurrentA = 0.0197;
  double harvesterUpdateInterval = 1.0;
  double harvesterPowerMin = 0.0;
  double harvesterPowerMax = 0.1;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  WifiMacHelper wifiMac;

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue("DsssRate1Mbps"),
    "ControlMode", StringValue("DsssRate1Mbps"));

  Ssid ssid = Ssid("energy-ssid");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 0.0, 0.0));
  posAlloc->Add(Vector(5.0, 0.0, 0.0));
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  NetDeviceContainer devices;
  devices.Add(staDevices);
  devices.Add(apDevices);
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
  EnergySourceContainer energySources = energySourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(txCurrentA));
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(rxCurrentA));
  DeviceEnergyModelContainer deviceModels;
  deviceModels = radioEnergyHelper.Install(devices, energySources);

  Ptr<UniformRandomVariable> randVar = CreateObject<UniformRandomVariable>();
  randVar->SetAttribute("Min", DoubleValue(harvesterPowerMin));
  randVar->SetAttribute("Max", DoubleValue(harvesterPowerMax));

  BasicEnergyHarvesterHelper harvesterHelper;
  harvesterHelper.Set("PeriodicHarvestedPowerUpdateInterval", TimeValue(Seconds(harvesterUpdateInterval)));
  harvesterHelper.Set("HarvestablePower", DoubleValue(harvesterPowerMax));
  harvesterHelper.Set("RandomHarvestedPower", PointerValue(randVar));
  EnergyHarvesterContainer harvesters;
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    harvesters.Add(harvesterHelper.Install(energySources.Get(i)));
  }

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(8*(1024))));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0023]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.9977]"));

  ApplicationContainer appSource = onoff.Install(nodes.Get(0));
  appSource.Start(Seconds(1.0));
  appSource.Stop(Seconds(simulationTime + 1.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer appSink = sink.Install(nodes.Get(1));
  appSink.Start(Seconds(0.0));
  appSink.Stop(Seconds(simulationTime + 2.0));

  Simulator::Schedule(Seconds(0.0), &LogStats, energySources.Get(1), DynamicCast<BasicEnergyHarvester>(harvesters.Get(1)));

  Simulator::Stop(Seconds(simulationTime + 2.0));
  Simulator::Run();

  Ptr<EnergySource> recvNodeEnergySource = energySources.Get(1);
  Ptr<BasicEnergyHarvester> recvNodeHarvester = DynamicCast<BasicEnergyHarvester>(harvesters.Get(1));

  std::cout << "----- Final Energy Statistics (Receiver Node) -----" << std::endl;
  std::cout << "Initial energy: " << recvNodeEnergySource->GetInitialEnergy() << " J" << std::endl;
  std::cout << "Total energy consumed: "
            << recvNodeEnergySource->GetInitialEnergy() - recvNodeEnergySource->GetRemainingEnergy()
            << " J" << std::endl;
  std::cout << "Residual energy: " << recvNodeEnergySource->GetRemainingEnergy() << " J" << std::endl;
  std::cout << "Last harvested power: " << recvNodeHarvester->GetHarvestedPower() << " W" << std::endl;

  Simulator::Destroy();
  return 0;
}