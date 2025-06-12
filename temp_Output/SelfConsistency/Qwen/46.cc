#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyDepletionTest");

class EnergyTracer {
public:
  static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_UNCOND("Time: " << Simulator::Now().GetSeconds() << "s Node Energy: " << remainingEnergy << " J");
    if (remainingEnergy <= 0.0) {
      NS_LOG_UNCOND("Node has entered sleep state due to energy depletion.");
    }
  }

  static void StateChangeCallback(std::string context, uint32_t nodeId, Time timestamp, DeviceEnergyModel::DeviceState newState) {
    NS_LOG_UNCOND("Time: " << timestamp.GetSeconds() << "s Node " << nodeId << " State changed to: " << newState);
  }
};

int main(int argc, char *argv[]) {
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 1024;
  double txPowerLevel = 16.0; // dBm
  double initialEnergy = 1000.0; // Joules
  double supplyVoltage = 3.0; // Volts

  CommandLine cmd(__FILE__);
  cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
  cmd.AddValue("initialEnergy", "Initial energy stored in node (Joules)", initialEnergy);
  cmd.AddValue("supplyVoltage", "Supply voltage (Volts)", supplyVoltage);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Mobility setup
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

  // Wifi setup
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("DsssRate11Mbps"),
                               "ControlMode", StringValue("DsssRate11Mbps"));

  wifiPhy.Set("TxPowerStart", DoubleValue(txPowerLevel));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerLevel));
  wifiPhy.Set("ChannelNumber", UintegerValue(1));

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Traffic generation
  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  // Sink application
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(10.0));

  // Energy model setup
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
  energySourceHelper.Set("SupplyVoltageV", DoubleValue(supplyVoltage));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.36));   // Typical value
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.18));   // Typical value
  radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.05)); // Typical value
  radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.002)); // Typical value

  EnergySourceContainer sources = energySourceHelper.Install(nodes);
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  // Tracing
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/ReceivedPowerW",
                  MakeCallback(&EnergyTracer::RemainingEnergy),
                  &EnergyTracer::RemainingEnergy);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<DeviceEnergyModel> model = deviceModels.Get(i);
    if (DynamicCast<WifiRadioEnergyModel>(model)) {
      DynamicCast<WifiRadioEnergyModel>(model)->TraceConnectWithoutContext(
          "StateChange", MakeCallback(&EnergyTracer::StateChangeCallback));
    }
  }

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-energy-phy-trace.tr"));
  deviceModels.EnableAsciiAll(ascii.CreateFileStream("wifi-energy-model-trace.tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}