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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPhyComparison");

class WifiPhySignalLogger : public Object
{
public:
  static TypeId GetTypeId()
  {
    return TypeId("WifiPhySignalLogger")
      .SetParent<Object>()
      .AddConstructor<WifiPhySignalLogger>();
  }

  void LogSignalAndNoise(Ptr<const SpectrumValue> signal, Ptr<const SpectrumValue> noise)
  {
    double avgSignal = Integral(*signal);
    double avgNoise = Integral(*noise);
    double snr = avgSignal / avgNoise;
    NS_LOG_UNCOND("Signal: " << avgSignal << " W | Noise: " << avgNoise << " W | SNR: " << snr);
  }
};

int main(int argc, char *argv[])
{
  uint32_t simulationTime = 10; // seconds
  double distance = 5.0;        // meters
  uint8_t mcs = 7;
  uint16_t channelWidth = 20;   // MHz
  bool shortGuardInterval = false;
  bool useTcp = false;
  bool pcapEnabled = false;
  std::string phyType = "Yans";

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between AP and STA", distance);
  cmd.AddValue("mcs", "MCS index (0-31)", mcs);
  cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable/disable short guard interval", shortGuardInterval);
  cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapEnabled);
  cmd.AddValue("phyType", "PHY model to use: Yans or Spectrum", phyType);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;

  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  YansWifiPhyHelper phyYans;
  SpectrumWifiPhyHelper phySpectrum;

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcs)));

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(9999));
  Config::SetDefault("ns3::WifiMac::Slot", TimeValue(MicroSeconds(9)));
  Config::SetDefault("ns3::WifiMac::Sifs", TimeValue(MicroSeconds(16)));
  Config::SetDefault("ns3::WifiMac::EifsNoDifs", TimeValue(MicroSeconds(16 + 9 + 9216 / 11e6 * 8e6)));
  Config::SetDefault("ns3::WifiMac::Pifs", TimeValue(MicroSeconds(9 + 16)));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;

  if (phyType == "Yans")
  {
    phyYans = YansWifiPhyHelper::Default();
    phyYans.SetChannel(WifiChannelHelper::DefaultChannel());
    phyYans.Set("ChannelWidth", UintegerValue(channelWidth));
    phyYans.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
    staDevices = wifi.Install(phyYans, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phyYans, mac, wifiApNode);
  }
  else if (phyType == "Spectrum")
  {
    phySpectrum = SpectrumWifiPhyHelper::Default();
    phySpectrum.SetChannel(WifiChannelHelper::DefaultSpectrumChannel());
    phySpectrum.Set("ChannelWidth", UintegerValue(channelWidth));
    phySpectrum.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
    staDevices = wifi.Install(phySpectrum, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phySpectrum, mac, wifiApNode);
  }
  else
  {
    NS_FATAL_ERROR("Invalid PHY type specified: " << phyType);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
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
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  ApplicationContainer serverApp;
  ApplicationContainer clientApp;

  if (useTcp)
  {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    serverApp = sink.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), 9));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1000));
    clientApp = onoff.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
  }
  else
  {
    UdpServerHelper server;
    serverApp = server.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterfaces.GetAddress(0), 49500);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
  }

  if (pcapEnabled)
  {
    phyYans.EnablePcap("wifi-phy-comparison-sta", staDevices.Get(0));
    phySpectrum.EnablePcap("wifi-phy-comparison-ap", apDevice.Get(0));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (phyType == "Spectrum")
  {
    Ptr<WifiPhySignalLogger> logger = CreateObject<WifiPhySignalLogger>();
    Config::Connect("/NodeList/*/DeviceList/*/Phy/State/RxSignal", MakeCallback(&WifiPhySignalLogger::LogSignalAndNoise, logger));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}