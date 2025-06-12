#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyAdHocWifiSimulation");

class EnergyTracer {
public:
  static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Remaining energy: " << remainingEnergy);
    if (remainingEnergy <= 0.0 && oldValue > 0.0) {
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Node has entered sleep state due to energy depletion");
    }
  }

  static void StateChange(uint32_t nodeId, WifiPhyState oldState, WifiPhyState newState) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Node " << nodeId << " state change: " << oldState << " -> " << newState);
  }
};

int main(int argc, char *argv[]) {
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 1024;
  double txPowerLevel = 16.0; // dBm
  double initialEnergy = 100.0; // Joules
  double supplyVoltage = 3.0; // Volts

  CommandLine cmd(__FILE__);
  cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
  cmd.AddValue("initialEnergy", "Initial energy in Joules", initialEnergy);
  cmd.AddValue("supplyVoltage", "Supply voltage for the device", supplyVoltage);
  cmd.Parse(argc, argv);

  Time simulationTime = Seconds(10.0);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());
  phyHelper.Set("TxPowerStart", DoubleValue(txPowerLevel));
  phyHelper.Set("TxPowerEnd", DoubleValue(txPowerLevel));
  phyHelper.Set("TxGain", DoubleValue(0));
  phyHelper.Set("RxGain", DoubleValue(0));
  phyHelper.Set("EnergyDetectionThreshold", DoubleValue(-96.0));
  phyHelper.Set("CcaMode1Threshold", DoubleValue(-99.0));

  WifiMacHelper macHelper;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211b);
  wifiHelper.SetRemoteStationManager("ns3::ArfWifiManager");

  macHelper.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifiHelper.Install(phyHelper, macHelper, nodes);

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

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(0.0));
  apps.Stop(simulationTime);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(simulationTime);

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
  energySourceHelper.Set("SupplyVoltageV", DoubleValue(supplyVoltage));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  EnergyModelContainer energyModels = radioEnergyHelper.Install(devices, energySourceHelper.Install(nodes));

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<BasicEnergySource> energySrc = DynamicCast<BasicEnergySource>(energyModels.Get(i)->GetEnergySource());
    energySrc->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&EnergyTracer::RemainingEnergy));
    Config::Connect("/NodeList/" + std::to_string(nodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/State",
                    MakeBoundCallback(&EnergyTracer::StateChange, nodes.Get(i)->GetId()));
  }

  Simulator::Stop(simulationTime);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}