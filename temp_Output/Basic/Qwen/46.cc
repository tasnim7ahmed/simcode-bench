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

NS_LOG_COMPONENT_DEFINE("EnergyAdhocWifi");

class EnergyTracer {
public:
  static void RemainingEnergy (Ptr<OutputStreamWrapper> stream, double oldValue, double remainingEnergy)
  {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << remainingEnergy << std::endl;
  }

  static void StateChangeNotification (std::string context, Time oldStateTime, DeviceEnergyModel::DeviceState newState)
  {
    NS_LOG_DEBUG(context << " state changed at " << oldStateTime.GetSeconds() << " to " << newState);
  }
};

int main(int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double dataRate = 2048; // in bps
  uint32_t packetSize = 1024;
  double txPowerLevel = 16.02; // dBm
  double initialEnergyJ = 1000.0;
  double supplyVoltageV = 3.0;
  std::string rateUnit = "bps";
  std::string energyTrFile = "remaining-energy.tr";
  std::string energyCtxFile = "state-transitions.ctx";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("dataRate", "Data rate in bps or kBps/KiBps based on rateUnit", dataRate);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
  cmd.AddValue ("initialEnergyJ", "Initial energy in joules", initialEnergyJ);
  cmd.AddValue ("supplyVoltageV", "Supply voltage in volts", supplyVoltageV);
  cmd.AddValue ("rateUnit", "Unit for data rate: bps, kBps, KiBps", rateUnit);
  cmd.Parse (argc, argv);

  if (rateUnit == "KiBps")
    {
      dataRate *= 1024;
    }
  else if (rateUnit == "kBps")
    {
      dataRate *= 1000;
    }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

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
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  apps = server.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(10.0));

  EnergySourceContainer sources;
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergyJ));
  basicSourceHelper.Set("SupplyVoltageV", DoubleValue(supplyVoltageV));
  sources = basicSourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.350)); // example value
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.197)); // example value
  radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.100)); // example value
  radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.010)); // example value

  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      Ptr<EnergySource> source = sources.Get(i);
      for (DeviceEnergyModelContainer::Iterator iter = deviceModels.Begin(); iter != deviceModels.End(); ++iter)
        {
          if ((*iter)->GetObject<Node>()->GetId() == node->GetId())
            {
              (*iter)->TraceConnectWithoutContext("RemainingEnergy", MakeBoundCallback(&EnergyTracer::RemainingEnergy, Create<OutputStreamWrapper>(energyTrFile, std::ios::out)));
              (*iter)->TraceConnect("StateChange", energyCtxFile, MakeCallback(&EnergyTracer::StateChangeNotification));
            }
        }
    }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}