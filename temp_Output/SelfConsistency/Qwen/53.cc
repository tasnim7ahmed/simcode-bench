#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationTest");

class WifiNetwork {
public:
  WifiNetwork(uint8_t channelNumber, std::string aggregationMode, uint32_t maxAmpduSize, uint32_t maxAmsduSize,
              bool enableRtsCts, uint32_t txopLimitUs, double distance)
      : m_channelNumber(channelNumber), m_aggregationMode(aggregationMode),
        m_maxAmpduSize(maxAmpduSize), m_maxAmsduSize(maxAmsduSize),
        m_enableRtsCts(enableRtsCts), m_txopLimitUs(txopLimitUs), m_distance(distance) {}

  void Setup();
  void InstallApplications();

  Ptr<Node> GetApNode() { return m_apNode; }
  Ptr<Node> GetStaNode() { return m_staNode; }
  uint8_t GetChannelNumber() const { return m_channelNumber; }

private:
  uint8_t m_channelNumber;
  std::string m_aggregationMode;
  uint32_t m_maxAmpduSize;
  uint32_t m_maxAmsduSize;
  bool m_enableRtsCts;
  uint32_t m_txopLimitUs;
  double m_distance;

  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDevices;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterfaces;
  Ipv4InterfaceContainer m_staInterfaces;

  void ConfigureAggregation(Ptr<WifiNetDevice> device);
  void TxopTrace(std::string context, Time duration);
};

void WifiNetwork::ConfigureAggregation(Ptr<WifiNetDevice> device) {
  if (m_aggregationMode == "disable") {
    device->SetAttribute("QosSupported", BooleanValue(false));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported",
                BooleanValue(false));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/LdpcSupported",
                BooleanValue(false));
    return;
  }

  device->SetAttribute("QosSupported", BooleanValue(true));

  if (m_aggregationMode.find("ampdu") != std::string::npos) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/MaxAmpduSize",
                UintegerValue(m_maxAmpduSize));
  } else {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/UseAmpdu", BooleanValue(false));
  }

  if (m_aggregationMode.find("amsdu") != std::string::npos) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/MaxAmsduSize",
                UintegerValue(m_maxAmsduSize));
  } else {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/UseAmsdu", BooleanValue(false));
  }
}

void WifiNetwork::TxopTrace(std::string context, Time duration) {
  NS_LOG_UNCOND(context << " TXOP Duration: " << duration.As(Time::US));
  std::cout << context << " TXOP Duration: " << duration.As(Time::US) << std::endl;
}

void WifiNetwork::Setup() {
  m_apNode.Create(1);
  m_staNode.Create(1);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  Ssid ssid = Ssid("wifi-" + std::to_string(m_channelNumber));
  WifiMacHelper mac;
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  m_staDevices = mac.Install(m_staNode.Get(0), phy.Create(m_staNode.Get(0)));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  m_apDevices = mac.Install(m_apNode.Get(0), phy.Create(m_apNode.Get(0)));

  // Configure aggregation
  ConfigureAggregation(DynamicCast<WifiNetDevice>(m_staDevices.Get(0)));
  ConfigureAggregation(DynamicCast<WifiNetDevice>(m_apDevices.Get(0)));

  // Set channel number
  phy.Set("ChannelNumber", UintegerValue(m_channelNumber));

  // RTS/CTS
  if (m_enableRtsCts) {
    wifi.EnableRts("ns3::RtsCtsWifiManager");
  }

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(m_distance), "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1." + std::to_string(m_channelNumber) + ".0", "255.255.255.0");
  m_apInterfaces = address.Assign(m_apDevices);
  m_staInterfaces = address.Assign(m_staDevices);

  // TXOP tracing
  Config::Connect("/NodeList/" + std::to_string(m_apNode.Get(0)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/BE_Txop/TxopDuration",
                  MakeCallback(&WifiNetwork::TxopTrace, this));
}

void WifiNetwork::InstallApplications() {
  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(m_staNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper onoff("ns3::TcpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));

  AddressValue remoteAddress(InetSocketAddress(m_staInterfaces.GetAddress(0), port));
  onoff.SetAttribute("Remote", remoteAddress);
  ApplicationContainer srcApp = onoff.Install(m_apNode.Get(0));
  srcApp.Start(Seconds(1.0));
  srcApp.Stop(Seconds(10.0));
}

int main(int argc, char *argv[]) {
  uint32_t txopLimitUs = 65535; // Default maximum TXOP duration in microseconds
  double distance = 10.0;       // meters
  bool enableRtsCts = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue("txopLimitUs", "TXOP Duration Limit (microseconds)", txopLimitUs);
  cmd.Parse(argc, argv);

  // Create four networks with different configurations
  WifiNetwork netA(36, "ampdu amsdu", 65536, 8192, enableRtsCts, txopLimitUs, distance);
  WifiNetwork netB(40, "disable", 0, 0, enableRtsCts, txopLimitUs, distance);
  WifiNetwork netC(44, "amsdu", 0, 8192, enableRtsCts, txopLimitUs, distance);
  WifiNetwork netD(48, "ampdu amsdu", 32768, 4096, enableRtsCts, txopLimitUs, distance);

  netA.Setup();
  netB.Setup();
  netC.Setup();
  netD.Setup();

  netA.InstallApplications();
  netB.InstallApplications();
  netC.InstallApplications();
  netD.InstallApplications();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  std::cout << "\nMaximum TXOP Durations:\n";
  std::cout << "Network A (channel 36): " << txopLimitUs << "us\n";
  std::cout << "Network B (channel 40): " << txopLimitUs << "us\n";
  std::cout << "Network C (channel 44): " << txopLimitUs << "us\n";
  std::cout << "Network D (channel 48): " << txopLimitUs << "us\n";

  Simulator::Destroy();
  return 0;
}