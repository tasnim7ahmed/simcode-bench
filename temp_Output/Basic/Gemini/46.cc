#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/internet-module.h"
#include "ns3/on-off-application.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyModelSimulation");

void DeviceEnergyStateChanged(std::string context,
                                 Ptr<EnergySource> energySource,
                                 EnergySource::State state) {
  NS_LOG_UNCOND(Simulator::Now().As(Time::GetPrintFormat()) << " " << context << " Device Energy State Change: " << state);
}

void RemainingEnergy(std::string context,
                       Ptr<EnergySource> energySource) {
  NS_LOG_UNCOND(Simulator::Now().As(Time::GetPrintFormat()) << " " << context << " Remaining Energy = " << energySource->GetRemainingEnergy());
}

int main(int argc, char *argv[]) {
  bool verbose = false;
  double simulationTime = 10;
  std::string dataRate = "11Mbps";
  uint32_t packetSize = 1024;
  double txPowerStart = 5.0;
  double txPowerEnd = 20.0;
  double txPowerStep = 5.0;

  double basicEnergyConsumption = 0.1;
  double txEnergyConsumptionPerBit = 0.0001;
  double rxEnergyConsumptionPerBit = 0.00005;
  double idleEnergyConsumption = 0.001;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("dataRate", "Data rate for the on-off application", dataRate);
  cmd.AddValue("packetSize", "Size of the packets for the on-off application", packetSize);
  cmd.AddValue("txPowerStart", "Starting transmission power (dBm)", txPowerStart);
  cmd.AddValue("txPowerEnd", "Ending transmission power (dBm)", txPowerEnd);
  cmd.AddValue("txPowerStep", "Step size for transmission power (dBm)", txPowerStep);
  cmd.AddValue("basicEnergyConsumption", "Basic energy consumption (W)", basicEnergyConsumption);
  cmd.AddValue("txEnergyConsumptionPerBit", "Tx energy consumption per bit (J)", txEnergyConsumptionPerBit);
  cmd.AddValue("rxEnergyConsumptionPerBit", "Rx energy consumption per bit (J)", rxEnergyConsumptionPerBit);
  cmd.AddValue("idleEnergyConsumption", "Idle energy consumption (W)", idleEnergyConsumption);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiEnergyModelSimulation", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(i.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate(dataRate));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simulationTime + 1));

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simulationTime + 1));

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("BasicEnergyConsumption", DoubleValue(basicEnergyConsumption));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxEnergyConsumptionPerBit", DoubleValue(txEnergyConsumptionPerBit));
  radioEnergyHelper.Set("RxEnergyConsumptionPerBit", DoubleValue(rxEnergyConsumptionPerBit));
  radioEnergyHelper.Set("IdleEnergyConsumption", DoubleValue(idleEnergyConsumption));
  radioEnergyHelper.Set("SleepEnergyConsumption", DoubleValue(0.00001));

  for (double txPower = txPowerStart; txPower <= txPowerEnd; txPower += txPowerStep) {
      wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
      wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

      EnergySourceContainer sources = basicSourceHelper.Install(nodes);
      radioEnergyHelper.Install(devices, sources);

      Ptr<EnergySource> sourceNode0 = sources.Get(0);
      sourceNode0->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
      sourceNode0->TraceConnect("DeviceEnergyState", MakeCallback(&DeviceEnergyStateChanged));

      Ptr<EnergySource> sourceNode1 = sources.Get(1);
      sourceNode1->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
      sourceNode1->TraceConnect("DeviceEnergyState", MakeCallback(&DeviceEnergyStateChanged));

      std::stringstream ss;
      ss << "wifi-energy-" << txPower << ".pcap";
      wifiPhy.EnablePcap(ss.str(), devices);

      Simulator::Stop(Seconds(simulationTime));
      Simulator::Run();
      Simulator::Destroy();
  }

  return 0;
}