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

class StationConfig {
public:
  StationConfig(uint8_t channel, bool enableAmpdu, uint32_t ampduMaxLength, bool enableAmsdu, uint32_t amsduMaxLength)
    : channel(channel),
      enableAmpdu(enableAmpdu),
      ampduMaxLength(ampduMaxLength),
      enableAmsdu(enableAmsdu),
      amsduMaxLength(amsduMaxLength) {}

  uint8_t channel;
  bool enableAmpdu;
  uint32_t ampduMaxLength;
  bool enableAmsdu;
  uint32_t amsduMaxLength;
};

static void
TxopTrace(std::string context, Time duration)
{
  std::cout << context << " TXOP Duration: " << duration.As(Time::MS) << std::endl;
}

int main(int argc, char *argv[])
{
  double distance = 1.0; // meters
  bool enableRtsCts = false;
  uint32_t txopLimitUs = 0; // 0 means default

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between AP and STA (m)", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS for all stations", enableRtsCts);
  cmd.AddValue("txopLimit", "TXOP limit in microseconds (0 for default)", txopLimitUs);
  cmd.Parse(argc, argv);

  NodeContainer apNodes;
  NodeContainer staNodes;
  apNodes.Create(4);
  staNodes.Create(4);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  std::vector<StationConfig> configs = {
    StationConfig(36, true, 65535, false, 0),
    StationConfig(40, false, 0, false, 0),
    StationConfig(44, false, 0, true, 8192),
    StationConfig(48, true, 32768, true, 4096)
  };

  NetDeviceContainer apDevices[4];
  NetDeviceContainer staDevices[4];

  for (uint32_t i = 0; i < 4; ++i)
  {
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::ConstantRateWifiManager/Aggregation", BooleanValue(configs[i].enableAmpdu || configs[i].enableAmsdu));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/UseAmpdu", BooleanValue(configs[i].enableAmpdu));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduMaximumLength", UintegerValue(configs[i].ampduMaxLength));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/UseAmsdu", BooleanValue(configs[i].enableAmsdu));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduMaximumLength", UintegerValue(configs[i].amsduMaxLength));

    if (txopLimitUs > 0)
    {
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Txop/Queue/MaxTxop", TimeValue(MicroSeconds(txopLimitUs)));
    }

    Ssid ssid = Ssid("wifi-default-" + std::to_string(i));
    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MilliSeconds(100)));

    phyHelper.Set("ChannelNumber", UintegerValue(configs[i].channel));
    phyHelper.Set("ChannelWidth", UintegerValue(20));

    apDevices[i] = wifi.Install(phyHelper, mac, apNodes.Get(i));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    staDevices[i] = wifi.Install(phyHelper, mac, staNodes.Get(i));

    if (enableRtsCts)
    {
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/RtsCtsEnabled", BooleanValue(true));
    }
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(distance),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces[4];
  Ipv4InterfaceContainer staInterfaces[4];

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  for (uint32_t i = 0; i < 4; ++i)
  {
    NodeContainer networkNodes(apNodes.Get(i), staNodes.Get(i));
    NetDeviceContainer devices = p2p.Install(networkNodes);
    apInterfaces[i] = address.Assign(devices.Get(0));
    staInterfaces[i] = address.Assign(devices.Get(1));
    address.NewNetwork();
  }

  UdpServerHelper serverHelper(4000);
  ApplicationContainer serverApps[4];
  ApplicationContainer clientApps[4];

  for (uint32_t i = 0; i < 4; ++i)
  {
    serverApps[i] = serverHelper.Install(staNodes.Get(i));
    serverApps[i].Start(Seconds(1.0));
    serverApps[i].Stop(Seconds(10.0));

    UdpClientHelper clientHelper(staInterfaces[i].GetAddress(0), 4000);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000000));
    clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    clientApps[i] = clientHelper.Install(apNodes.Get(i));
    clientApps[i].Start(Seconds(1.0));
    clientApps[i].Stop(Seconds(10.0));

    Config::ConnectWithoutContext("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) +
                                  "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Txop/StartTxop",
                                  MakeCallback(&TxopTrace));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  for (uint32_t i = 0; i < 4; ++i)
  {
    uint64_t totalRx = DynamicCast<UdpServer>(serverApps[i].Get(0))->GetReceived();
    double throughput = (totalRx * 8) / (10.0 - 1.0) / 1000000.0; // Mbps
    std::cout << "Network " << i << " Throughput: " << throughput << " Mbps" << std::endl;
  }

  Simulator::Destroy();
  return 0;
}