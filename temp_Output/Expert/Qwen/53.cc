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
  uint8_t channel;
  bool enableAmpdu;
  uint32_t ampduMaxSize;
  bool enableAmsdu;
  uint32_t amsduMaxSize;
  bool enableRtsCts;
  uint32_t txopLimitUs;
};

void
TraceTxop (std::string context, Time duration)
{
  std::cout << context << " TXOP Duration: " << duration.As(Time::US) << std::endl;
}

int main(int argc, char *argv[])
{
  double distance = 1.0; // meters
  bool enableRtsCts = false;
  uint32_t txopLimitUs = 0; // microseconds

  CommandLine cmd (__FILE__);
  cmd.AddValue ("distance", "Distance between AP and STA (m)", distance);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS for all stations", enableRtsCts);
  cmd.AddValue ("txopLimitUs", "TXOP Limit in microseconds (0 for default)", txopLimitUs);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (enableRtsCts ? "0" : "2275"));

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  InternetStackHelper stack;
  Ipv4AddressHelper address;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  std::vector<StationConfig> configs = {
    {36, true, 65535, false, 0, enableRtsCts, txopLimitUs},
    {40, false, 0, false, 0, enableRtsCts, txopLimitUs},
    {44, false, 0, true, 8192, enableRtsCts, txopLimitUs},
    {48, true, 32768, true, 4096, enableRtsCts, txopLimitUs}
  };

  NodeContainer apNodes;
  NodeContainer staNodes;
  apNodes.Create(4);
  staNodes.Create(4);

  NetDeviceContainer apDevices;
  NetDeviceContainer staDevices;
  Ipv4InterfaceContainer apInterfaces;
  Ipv4InterfaceContainer staInterfaces;

  for (uint32_t i = 0; i < 4; ++i)
    {
      auto config = configs[i];

      Ssid ssid = Ssid("wifi-network-" + std::to_string(i));
      WifiMacHelper mac;
      mac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(MicroSeconds(102400)),
                  "EnableBeaconJitter", BooleanValue(false));

      phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      phy.SetChannel(channelHelper.Create());
      phy.Set("ChannelNumber", UintegerValue(config.channel));
      phy.Set("RxGain", DoubleValue(0));
      phy.Set("TxPowerStart", DoubleValue(16));
      phy.Set("TxPowerEnd", DoubleValue(16));
      phy.Set("TxPowerLevels", UintegerValue(1));

      apDevices.Add(wifi.Install(phy, mac, apNodes.Get(i)));

      mac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

      if (config.enableAmpdu)
        {
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Ampdu", BooleanValue(true));
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(config.ampduMaxSize));
        }
      else
        {
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Ampdu", BooleanValue(false));
        }

      if (config.enableAmsdu)
        {
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Amsdu", BooleanValue(true));
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmsduSize", UintegerValue(config.amsduMaxSize));
        }
      else
        {
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Amsdu", BooleanValue(false));
        }

      if (config.txopLimitUs > 0)
        {
          Config::Set("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop/Queue/MaxDelay", TimeValue(MicroSeconds(config.txopLimitUs)));
          Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop/Queue/MaxDelay", TimeValue(MicroSeconds(config.txopLimitUs)));
        }

      staDevices.Add(wifi.Install(phy, mac, staNodes.Get(i)));

      Config::Connect ("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/TraceTxopUsed", MakeCallback(&TraceTxop));
    }

  mobility.Install(apNodes);
  mobility.Install(staNodes);

  stack.Install(apNodes);
  stack.Install(staNodes);

  address.SetBase("192.168.1.0", "255.255.255.0");
  apInterfaces = p2p.Install(apNodes.Get(0), apNodes.Get(1));
  apInterfaces.Add(p2p.Install(apNodes.Get(1), apNodes.Get(2)));
  apInterfaces.Add(p2p.Install(apNodes.Get(2), apNodes.Get(3)));

  for (uint32_t i = 0; i < 4; ++i)
    {
      address.NewNetwork();
      Ipv4InterfaceContainer apIf = address.Assign(apDevices.Get(i));
      address.NewNetwork();
      Ipv4InterfaceContainer staIf = address.Assign(staDevices.Get(i));
      apInterfaces.Add(apIf);
      staInterfaces.Add(staIf);
    }

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(staNodes);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client;
  client.SetAttribute("RemoteAddress", AddressValue(staInterfaces.GetAddress(0, 0)));
  client.SetAttribute("RemotePort", UintegerValue(9));
  client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  client.SetAttribute("PacketSize", UintegerValue(1400));

  ApplicationContainer clientApps = client.Install(apNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  for (uint32_t i = 0; i < 4; ++i)
    {
      double rxBytes = DynamicCast<UdpServer>(serverApps.Get(i))->GetReceived());
      double throughput = (rxBytes * 8.0) / (9.0 * 1e6); // Mbps
      std::cout << "Network " << i << ": Throughput = " << throughput << " Mbps" << std::endl;
    }

  Simulator::Destroy();
  return 0;
}