#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiEnergySimulation");

class EnergyDepletionTracer : public Object
{
public:
  static TypeId GetTypeId(void)
  {
    return TypeId("EnergyDepletionTracer")
        .SetParent<Object>()
        .AddConstructor<EnergyDepletionTracer>();
  }

  void TraceStateChange(Ptr<const EnergySource> source, NodeEnergyModel::NodeEnergyState newState)
  {
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s Node State Changed to: " << newState);
    if (newState == NodeEnergyModel::SLEEP)
    {
      NS_LOG_INFO(Simulator::Now().GetSeconds() << "s Node entered sleep due to energy depletion");
    }
  }

  void TraceRemainingEnergy(Ptr<const EnergySource> source)
  {
    double remainingEnergy = source->GetRemainingEnergy();
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s Remaining Energy: " << remainingEnergy << " J");
  }
};

int main(int argc, char *argv[])
{
  std::string phyMode("DsssRate1Mbps");
  uint32_t packetSize = 1000;
  double dataRate = 1.0; // Mbps
  double txPowerLevel = 16.02; // dBm - default for 802.11b
  double simulationTime = 10.0; // seconds
  double initialEnergy = 1000.0; // Joules

  CommandLine cmd(__FILE__);
  cmd.AddValue("dataRate", "Data rate in Mbps", dataRate);
  cmd.AddValue("packetSize", "Packet Size in bytes", packetSize);
  cmd.AddValue("txPowerLevel", "Transmission Power Level (dBm)", txPowerLevel);
  cmd.AddValue("initialEnergy", "Initial Energy in Joules", initialEnergy);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(packetSize));
  Config::SetDefault("ns3::OnOffApplication::DataRate", DataRateValue(DataRate(dataRate, DataRateUnit::MBps)));

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

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
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simulationTime));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
  energySourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.350)); // typical value
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.192)); // typical value
  radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.050)); // typical value
  radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.001)); // typical value

  EnergyModelContainer energyModel = radioEnergyHelper.Install(devices, energySourceHelper.Install(nodes));

  EnergyDepletionTracer tracer;
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/NodeEnergyModel/StateChange",
                  MakeCallback(&EnergyDepletionTracer::TraceStateChange, &tracer));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/NodeEnergyModel/RemainingEnergy",
                  MakeCallback(&EnergyDepletionTracer::TraceRemainingEnergy, &tracer));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("remaining-energy-trace.tr");
  energyModel.Get(0)->TraceConnectWithoutContext("RemainingEnergy", MakeBoundCallback(&AsciiTraceHelper::DefaultDequeueSinkDouble, stream));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}