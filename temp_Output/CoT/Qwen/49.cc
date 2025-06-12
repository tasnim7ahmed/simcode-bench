#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterfererSimulation");

class Interferer : public SpectrumPhy
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("Interferer")
                          .SetParent<SpectrumPhy>()
                          .AddConstructor<Interferer>();
    return tid;
  }

  void SetTxPower(double power)
  {
    m_power = power;
  }

  void StartTransmission()
  {
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue>(m_channel->GetSpectrumModel());
    *txPsd = m_power / (m_channel->GetSpectrumModel()->GetNumBands());
    m_channel->StartTx(txPsd);
  }

private:
  virtual void DoDispose()
  {
    m_channel = nullptr;
    SpectrumPhy::DoDispose();
  }

  Ptr<const SpectrumModel> GetRxSpectrumModel() const override
  {
    return m_channel ? m_channel->GetSpectrumModel() : nullptr;
  }

  Ptr<SpectrumChannel> m_channel;
  double m_power{0.0};
};

int main(int argc, char *argv[])
{
  uint32_t simulationTime = 10;
  double distance = 5.0;
  uint8_t mcsIndex = 4;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = false;
  double interfererPower = -60.0; // dBm
  std::string wifiType = "ns3::YansWifiPhy";
  std::string errorModel = "ns3::NistErrorRateModel";
  bool pcapTracing = false;
  bool enableTcp = true;
  bool enableUdp = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
  cmd.AddValue("distance", "Distance between nodes (meters)", distance);
  cmd.AddValue("mcsIndex", "MCS index", mcsIndex);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
  cmd.AddValue("interfererPower", "Interferer power in dBm", interfererPower);
  cmd.AddValue("wifiType", "WiFi type: 'ns3::YansWifiPhy' or 'ns3::SpectrumWifiPhy'", wifiType);
  cmd.AddValue("errorModel", "Error rate model", errorModel);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue("tcp", "Enable TCP traffic", enableTcp);
  cmd.AddValue("udp", "Enable UDP traffic", enableUdp);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("9999"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(WifiModeFactory().CreateWifiMode("OfdmRate" + std::to_string(channelWidth) + "MbpsBW" + std::to_string(channelWidth) + "MHz").GetString()));
  Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
  Config::SetDefault("ns3::WifiPhy::ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
  Config::SetDefault("ns3::WifiPhy::FrameCaptureModel", StringValue("ns3::SimpleFrameCaptureModel"));
  Config::SetDefault("ns3::WifiPhy::PreambleDetectionModel", StringValue("ns3::SimplePreambleDetectionModel"));
  Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(errorModel));

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);
  NodeContainer interfererNode;
  interfererNode.Create(1);

  YansWifiChannelHelper channelYans;
  channelYans.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channelYans.AddPropagationLoss("ns3::FriisPropagationLossModel");

  YansWifiPhyHelper phyYans;
  phyYans.SetChannel(channelYans.Create());

  SpectrumChannelHelper channelSpectrum = SpectrumChannelHelper::Default();
  channelSpectrum.SetChannel("ns3::MultiModelSpectrumChannel");

  SpectrumWifiPhyHelper phySpectrum;
  phySpectrum.SetChannel(channelSpectrum.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcsIndex)));

  NetDeviceContainer staDevice;
  NetDeviceContainer apDevice;

  if (wifiType == "ns3::YansWifiPhy")
  {
    phyYans.ConfigureStandard(WIFI_STANDARD_80211n);
    mac.SetType("ns3::StaWifiMac");
    staDevice = wifi.Install(phyYans, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac");
    apDevice = wifi.Install(phyYans, mac, wifiApNode);
  }
  else if (wifiType == "ns3::SpectrumWifiPhy")
  {
    phySpectrum.ConfigureStandard(WIFI_STANDARD_80211n);
    mac.SetType("ns3::StaWifiMac");
    staDevice = wifi.Install(phySpectrum, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac");
    apDevice = wifi.Install(phySpectrum, mac, wifiApNode);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNode);
  wifiStaNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));

  mobility.Install(wifiApNode);
  wifiApNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));

  mobility.Install(interfererNode);
  interfererNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));

  if (wifiType == "ns3::SpectrumWifiPhy")
  {
    Ptr<Interferer> interfererPhy = CreateObject<Interferer>();
    interfererPhy->SetChannel(phySpectrum.GetChannel());
    interfererPhy->SetTxPower(WifiConvert::DbmToW(interfererPower));
    interfererPhy->StartTransmission();
  }

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  uint16_t port = 9;

  if (enableTcp)
  {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1000));
    ApplicationContainer clientApp = onoff.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
  }

  if (enableUdp)
  {
    UdpEchoServerHelper echoServer(port + 1);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port + 1);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = echoClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));
  }

  if (pcapTracing)
  {
    phyYans.EnablePcap("wifi-interferer-simulation", apDevice.Get(0));
    phyYans.EnablePcap("wifi-interferer-simulation", staDevice.Get(0));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}