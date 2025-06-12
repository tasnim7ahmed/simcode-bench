#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPhyTest");

class PhyStatsHelper : public StatsCalculator
{
public:
  static TypeId GetTypeId(void);
  void LogSignalAndNoise(Ptr<const Packet> packet, Ptr<WifiPhy> phy);
};

TypeId
PhyStatsHelper::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::PhyStatsHelper")
    .SetParent<StatsCalculator>()
    .AddConstructor<PhyStatsHelper>();
  return tid;
}

void
PhyStatsHelper::LogSignalAndNoise(Ptr<const Packet> packet, Ptr<WifiPhy> phy)
{
  NS_LOG_INFO("Packet received. Signal: " << phy->GetRxPowerW(packet) << " W, Noise: " << phy->GetNoiseFigure() << " dBm");
}

int
main(int argc, char *argv[])
{
  uint32_t payloadSize = 1472;           // bytes
  double simulationTime = 10;            // seconds
  double distance = 10.0;                // meters
  std::string phyMode("HtMcs7");         // default MCS
  bool enablePcap = false;
  bool useUdp = true;
  bool useSpectrumPhy = false;
  uint8_t channelWidth = 20;
  bool shortGuardInterval = false;
  std::string dataRate = "650Mbps";

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes (m)", distance);
  cmd.AddValue("phyMode", "Wifi Phy mode (e.g., HtMcs0, HtMcs7)", phyMode);
  cmd.AddValue("enablePcap", "Enable pcap capture", enablePcap);
  cmd.AddValue("useUdp", "Use UDP instead of TCP", useUdp);
  cmd.AddValue("useSpectrumPhy", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrumPhy);
  cmd.AddValue("channelWidth", "Channel width (MHz): 20, 40", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211n);

  if (useSpectrumPhy)
    {
      wifiHelper.SetPhy(WIFI_PHY_LAYER_SPECTRUM_WIFI_PHY);
    }
  else
    {
      wifiHelper.SetPhy(WIFI_PHY_LAYER_ATH9KHTC);
    }

  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(phyMode),
                                     "ControlMode", StringValue(phyMode));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

  Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
  Config::SetDefault("ns3::WifiPhy::ShortGuardInterval", BooleanValue(shortGuardInterval));

  NetDeviceContainer staDevice;
  NetDeviceContainer apDevice;

  Ssid ssid = Ssid("wifi-test-phy");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
  staDevice = wifiHelper.Install(channel.Create(), wifiStaNode);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  apDevice = wifiHelper.Install(channel.Create(), wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  Ipv4InterfaceContainer apInterface;
  staInterface = address.Assign(staDevice);
  apInterface = address.Assign(apDevice);

  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper(useUdp ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime));

  OnOffHelper onoff(useUdp ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer srcApp = onoff.Install(wifiApNode.Get(0));
  srcApp.Start(Seconds(1.0));
  srcApp.Stop(Seconds(simulationTime));

  PhyStatsHelper phyStats;
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxEnd", MakeCallback(&PhyStatsHelper::LogSignalAndNoise, &phyStats));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (enablePcap)
    {
      wifiHelper.EnablePcap("wifi_phy_test", apDevice.Get(0));
      wifiHelper.EnablePcap("wifi_phy_test", staDevice.Get(0));
    }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().GetObject())->FindFlow(iter->first);
      std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
      std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps" << std::endl;
    }

  Simulator::Destroy();
  return 0;
}