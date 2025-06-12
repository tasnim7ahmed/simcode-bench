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

NS_LOG_COMPONENT_DEFINE("EnergyAdhocWifiSimulation");

class EnergyDepletionCallback : public EnergySource::StateChangeCallback
{
public:
  void operator()(const EnergySource* energySource) override
  {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Node entered sleep state due to energy depletion");
  }
};

int main(int argc, char *argv[])
{
  std::string phyMode = "DsssRate11Mbps";
  double dataRate = 2048; // in bps
  uint32_t packetSize = 1024;
  double txPowerLevel = 16.0; // dBm
  double initialEnergyJ = 1000.0;
  double supplyVoltageV = 3.0;
  double idleCurrentA = 0.05;
  double rxCurrentA = 0.197;
  double txCurrentA = 0.232;
  std::string wifiMacType = "ns3::AdhocWifiMac";

  CommandLine cmd(__FILE__);
  cmd.AddValue("dataRate", "Data rate in bps", dataRate);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
  cmd.AddValue("initialEnergyJ", "Initial energy in Joules", initialEnergyJ);
  cmd.AddValue("supplyVoltageV", "Supply voltage in Volts", supplyVoltageV);
  cmd.AddValue("idleCurrentA", "Idle current in Amps", idleCurrentA);
  cmd.AddValue("rxCurrentA", "Receive current in Amps", rxCurrentA);
  cmd.AddValue("txCurrentA", "Transmit current in Amps", txCurrentA);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds((double(packetSize * 8) / dataRate));

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> channel = wifiChannel.Create();
  wifiPhy.SetChannel(channel);
  wifiPhy.Set("TxPowerStart", DoubleValue(txPowerLevel));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerLevel));
  wifiPhy.Set("TxGain", DoubleValue(0));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("RxNoiseFigure", DoubleValue(10));
  wifiPhy.Set("CcaMode1Threshold", DoubleValue(-90));
  wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  WifiMacHelper wifiMac;
  wifiMac.SetType(wifiMacType);

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(10.0));

  UdpEchoServerHelper echo(9);
  ApplicationContainer serverApps = echo.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergyJ));
  energySourceHelper.Set("BasicEnergySourceSupplyVoltageV", DoubleValue(supplyVoltageV));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(txCurrentA));
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(rxCurrentA));
  radioEnergyHelper.Set("IdleCurrentA", DoubleValue(idleCurrentA));
  radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.001));

  EnergySourceContainer sources = energySourceHelper.Install(nodes);
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State",
                  MakeCallback([](std::string path, const EnumValue &value) {
                    std::ofstream fout("wifi-state-trace.txt", std::ios_base::app);
                    if (fout.is_open())
                      {
                        fout << Simulator::Now().GetSeconds() << " " << value.GetValue() << std::endl;
                        fout.close();
                      }
                  }));

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      Ptr<EnergySource> source = sources.Get(i);
      source->SetStateChangeCallback(EnergyDepletionCallback());
    }

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-phy-trace.tr"));
  wifi.MacMacTxTrace.ConnectWithoutContext(ascii.CreateFileStream("mac-tx-trace.tr"));
  wifi.MacMacRxTrace.ConnectWithoutContext(ascii.CreateFileStream("mac-rx-trace.tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}