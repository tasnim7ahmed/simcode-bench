#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

class InterfererHelper : public SpectrumValueHelper
{
public:
  InterfererHelper();
  void SetTxPower(double watts);
  void SetChannel(Ptr<SpectrumChannel> channel);
  void CreateAndInstall(NodeContainer interferers, uint32_t numPackets, Time packetInterval);

private:
  double m_powerWatts;
};

InterfererHelper::InterfererHelper()
    : SpectrumValueHelper(WifiSpectrumValue5MhzFactory())
{
  m_powerWatts = 0.1;
}

void InterfererHelper::SetTxPower(double watts)
{
  m_powerWatts = watts;
}

void InterfererHelper::SetChannel(Ptr<SpectrumChannel> channel)
{
  m_channel = channel;
}

void InterfererHelper::CreateAndInstall(NodeContainer interferers, uint32_t numPackets, Time packetInterval)
{
  for (NodeContainer::Iterator i = interferers.Begin(); i != interferers.End(); ++i)
  {
    Ptr<ConstantPositionMobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
    (*i)->AggregateObject(mobility);

    Ptr<SpectrumTransmitter> tx = CreateObject<SpectrumTransmitter>();
    tx->SetAttribute("Power", DoubleValue(m_powerWatts));
    tx->SetAttribute("TxGain", DoubleValue(0));
    tx->SetAttribute("Antenna", PointerValue(CreateObject<IsotropicAntennaModel>()));
    tx->SetChannel(m_channel);
    (*i)->AddApplication(tx);

    Simulator::ScheduleNow(&SpectrumTransmitter::Start, tx, Seconds(0), packetInterval, numPackets);
  }
}

int main(int argc, char *argv[])
{
  uint32_t simulationTime = 10;
  double distance = 10.0;
  uint32_t mcsIndex = 4;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = false;
  double waveformPower = 0.1;
  std::string wifiType = "Yans";
  std::string errorModel = "ns3::NistErrorRateModel";
  bool enablePcap = false;
  bool useTcp = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("mcsIndex", "Wi-Fi MCS index", mcsIndex);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
  cmd.AddValue("waveformPower", "Non-WiFi interferer power in Watts", waveformPower);
  cmd.AddValue("wifiType", "Wi-Fi propagation type: Yans or Spectrum", wifiType);
  cmd.AddValue("errorModel", "Error model to use", errorModel);
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("useTcp", "Use TCP if true, else UDP", useTcp);
  cmd.Parse(argc, argv);

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(1);

  NodeContainer interferers;
  interferers.Create(1);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNodes);

  SpectrumChannelHelper spectrumChannelHelper = SpectrumChannelHelper::Default();
  spectrumChannelHelper.SetChannel("ns3::MultiModelSpectrumChannel");

  YansWifiChannelHelper yansWifiChannel = YansWifiChannelHelper::Default();

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                     "ControlMode", StringValue("HtMcs0"));

  Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
  Config::SetDefault("ns3::WifiPhy::ShortGuardInterval", BooleanValue(shortGuardInterval));
  Config::SetDefault("ns3::WifiPhy::TxPowerStart", DoubleValue(16));
  Config::SetDefault("ns3::WifiPhy::TxPowerEnd", DoubleValue(16));
  Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(errorModel));

  YansWifiPhyHelper phy;
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  if (wifiType == "Yans")
  {
    phy.SetChannel(yansWifiChannel.Create());
  }
  else
  {
    phy.SetChannel(spectrumChannelHelper.Create());
  }

  Ssid ssid = Ssid("wifi-interference-test");
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifiHelper.Install(phy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifiHelper.Install(phy, wifiMac, apNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

  uint16_t port = 9;

  if (useTcp)
  {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(staNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("5Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer app = onoff.Install(apNode.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime - 0.1));
  }
  else
  {
    UdpServerHelper server(port);
    ApplicationContainer sinkApp = server.Install(staNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(Time("0.001s")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = client.Install(apNode.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime - 0.1));
  }

  InterfererHelper interfererHelper;
  interfererHelper.SetTxPower(waveformPower);
  if (wifiType == "Yans")
  {
    interfererHelper.SetChannel(yansWifiChannel.GetChannel());
  }
  else
  {
    interfererHelper.SetChannel(spectrumChannelHelper.GetChannel());
  }
  interfererHelper.CreateAndInstall(interferers, 1000, MilliSeconds(10));

  if (enablePcap)
  {
    phy.EnablePcap("wifi_interference_ap", apDevices.Get(0));
    phy.EnablePcap("wifi_interference_sta", staDevices.Get(0));
  }

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk",
                  MakeCallback([](std::string path, Ptr<const Packet> packet, const WifiConstPsduMap &psdus, RxSignalInfo signalInfo, WifiTxVector txVector, std::vector<bool> perMpduStatus) {
                    NS_LOG_UNCOND("RxOk: RSSI=" << signalInfo.rssi << "dBm SNR=" << signalInfo.snr << "dB Rate=" << txVector.GetDataRate(channelWidth, shortGuardInterval));
                  }));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}