#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiEnergyExample");

std::ofstream energyLog;
std::ofstream stateLog;

void
RemainingEnergyTrace(double oldValue, double newValue)
{
  Simulator::ScheduleNow(&Simulator::Now); // For correct tracing in multithreaded runs
  energyLog << Simulator::Now().GetSeconds() << "\t" << newValue << std::endl;
}

void
DeviceStateChangedTrace(int32_t oldState, int32_t newState)
{
  Simulator::ScheduleNow(&Simulator::Now);
  stateLog << Simulator::Now().GetSeconds() << "\t" << oldState << "\t" << newState << std::endl;
  if (newState == WifiPhyState::PHY_SLEEP)
    {
      NS_LOG_UNCOND("Node entered sleep state at time " << Simulator::Now().GetSeconds());
    }
}

void
DepletionCallback()
{
  NS_LOG_UNCOND("Node energy depleted at time " << Simulator::Now().GetSeconds());
}

int
main(int argc, char *argv[])
{
  double simTime = 20.0;
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 512;
  double txPower = 16.0; // in dBm
  double basicEnergy = 1.0; // initial energy in Joules
  double idleCurrent = 0.273; // in A
  double txCurrent = 0.380;   // in A
  double rxCurrent = 0.313;   // in A

  CommandLine cmd;
  cmd.AddValue("dataRate", "Application data rate (e.g. '1Mbps')", dataRate);
  cmd.AddValue("packetSize", "Application packet size in bytes", packetSize);
  cmd.AddValue("txPower", "Transmission power in dBm", txPower);
  cmd.AddValue("basicEnergy", "Initial energy in Joules", basicEnergy);
  cmd.AddValue("idleCurrent", "Idle current for Wifi radio (A)", idleCurrent);
  cmd.AddValue("txCurrent", "TX current (A)", txCurrent);
  cmd.AddValue("rxCurrent", "RX current (A)", rxCurrent);
  cmd.Parse(argc, argv);

  energyLog.open("energy.log");
  stateLog.open("state.log");

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Place nodes at different points
  Ptr<MobilityModel> mm1 = nodes.Get(0)->GetObject<MobilityModel>();
  Ptr<MobilityModel> mm2 = nodes.Get(1)->GetObject<MobilityModel>();
  mm1->SetPosition(Vector(0.0, 0.0, 0.0));
  mm2->SetPosition(Vector(5.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Energy Model setup
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(basicEnergy));
  EnergySourceContainer sources = energySourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(txCurrent));
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(rxCurrent));
  radioEnergyHelper.Set("IdleCurrentA", DoubleValue(idleCurrent));

  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  // Trace and callbacks
  Ptr<BasicEnergySource> src = DynamicCast<BasicEnergySource>(sources.Get(0));
  src->TraceConnectWithoutContext("RemainingEnergy",
                                  MakeCallback(&RemainingEnergyTrace));
  Ptr<WifiRadioEnergyModel> wifiEnergy = DynamicCast<WifiRadioEnergyModel>(deviceModels.Get(0));
  wifiEnergy->TraceConnectWithoutContext("State",
                                         MakeCallback(&DeviceStateChangedTrace));
  src->SetEnergyDepletionCallback(MakeCallback(&DepletionCallback));

  // Application: OnOff
  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate(dataRate), packetSize);
  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simTime));

  PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simTime));

  wifiPhy.EnablePcap("adhoc-energy", devices);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  energyLog.close();
  stateLog.close();
  return 0;
}