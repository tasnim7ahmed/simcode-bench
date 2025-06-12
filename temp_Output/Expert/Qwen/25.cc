#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsAnalysis");

class WifiMcsSimulation
{
public:
  WifiMcsSimulation();
  void Run(uint32_t nStations, uint32_t mcsValue, uint16_t channelWidth, Time simulationTime,
           uint32_t payloadSize, bool enableRtsCts, bool enableUlOfdma, bool enableDlMu,
           bool enableExtendedBlockAck, DataRate targetDataRate, bool useTcp, double distance,
           uint8_t guardInterval, uint32_t band, bool spectrumPhy);

private:
  NodeContainer wifiStaNodes;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
};

WifiMcsSimulation::WifiMcsSimulation()
{
  wifiStaNodes.Create(1);
  wifiApNode.Create(1);
}

void
WifiMcsSimulation::Run(uint32_t nStations, uint32_t mcsValue, uint16_t channelWidth,
                       Time simulationTime, uint32_t payloadSize, bool enableRtsCts,
                       bool enableUlOfdma, bool enableDlMu, bool enableExtendedBlockAck,
                       DataRate targetDataRate, bool useTcp, double distance, uint8_t guardInterval,
                       uint32_t band, bool spectrumPhy)
{
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "0" : "999999"));
  Config::SetDefault("ns3::WifiMac::Slot", TimeValue(MicroSeconds(9)));
  Config::SetDefault("ns3::WifiMac::Sifs", TimeValue(MicroSeconds(16)));
  Config::SetDefault("ns3::WifiMac::EifsNoDifs", TimeValue(MicroSeconds(80)));
  Config::SetDefault("ns3::WifiMac::AckTimeout", TimeValue(MicroSeconds(80)));
  Config::SetDefault("ns3::WifiMac::CtsTimeout", TimeValue(MicroSeconds(80)));
  Config::SetDefault("ns3::WifiMac::BasicBlockAckTimeout", TimeValue(MicroSeconds(80)));
  Config::SetDefault("ns3::WifiMac::CompressedBlockAckTimeout", TimeValue(MicroSeconds(80)));
  Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
  Config::SetDefault("ns3::WifiPhy::GuardInterval", UintegerValue(guardInterval));
  Config::SetDefault("ns3::WifiPhy::PreambleDetectionModel", StringValue("ns3::NistErrorRateModel"));

  if (enableExtendedBlockAck)
    {
      Config::SetDefault("ns3::WifiMac::UseExplicitBarAfterMissedDlMuBar", BooleanValue(true));
    }

  YansWifiChannelHelper channel;
  YansWifiPhyHelper phy;
  if (spectrumPhy)
    {
      channel = SpectrumWifiChannelHelper::Default();
    }
  else
    {
      channel = YansWifiChannelHelper::Default();
    }

  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);

  std::ostringstream oss;
  oss << "HeMcs" << mcsValue;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(oss.str()),
                               "ControlMode", StringValue(oss.str()));

  Ssid ssid = Ssid("wifi-default");
  WifiMacHelper mac;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(MilliSeconds(100)));

  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(nStations),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  apInterface = address.Assign(apDevice);
  staInterfaces = address.Assign(staDevices);

  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (useTcp)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
      serverApps.Add(sink.Install(wifiApNode.Get(0)));

      for (uint32_t i = 0; i < nStations; ++i)
        {
          BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), 9));
          source.SetAttribute("MaxBytes", UintegerValue(0));
          source.SetAttribute("SendSize", UintegerValue(payloadSize));
          clientApps.Add(source.Install(wifiStaNodes.Get(i)));
        }
    }
  else
    {
      UdpServerHelper server;
      serverApps.Add(server.Install(wifiApNode.Get(0)));

      UdpClientHelper client(apInterface.GetAddress(0), 9);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(Seconds(1) / targetDataRate.GetBitRate() * payloadSize * 8));
      client.SetAttribute("PacketSize", UintegerValue(payloadSize));
      clientApps.Add(client.Install(wifiStaNodes));
    }

  serverApps.Start(Seconds(1.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(simulationTime + Seconds(1));
  clientApps.Stop(simulationTime + Seconds(1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(1) + simulationTime);
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Get())->FindFlow(iter->first);
      std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress
                << " MCS: " << mcsValue << " ChannelWidth: " << channelWidth << " Throughput: " << iter->second.rxBytes * 8.0 / simulationTime.GetSeconds() / 1e6
                << " Mbps" << std::endl;
    }

  Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
  uint32_t nStations = 5;
  uint32_t mcsValue = 7;
  uint16_t channelWidth = 80;
  Time simulationTime = Seconds(10);
  uint32_t payloadSize = 1472;
  bool enableRtsCts = false;
  bool enableUlOfdma = true;
  bool enableDlMu = true;
  bool enableExtendedBlockAck = true;
  DataRate targetDataRate("100Mbps");
  bool useTcp = false;
  double distance = 5.0;
  uint8_t guardInterval = 800;
  uint32_t band = 5;
  bool spectrumPhy = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nStations", "Number of stations", nStations);
  cmd.AddValue("mcsValue", "MCS value (0-11)", mcsValue);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
  cmd.AddValue("enableDlMu", "Enable DL MU", enableDlMu);
  cmd.AddValue("enableExtendedBlockAck", "Enable extended block ack", enableExtendedBlockAck);
  cmd.AddValue("targetDataRate", "Target data rate", targetDataRate);
  cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("guardInterval", "Guard interval in nanoseconds", guardInterval);
  cmd.AddValue("band", "Wi-Fi frequency band (2.4, 5 or 6 GHz)", band);
  cmd.AddValue("spectrumPhy", "Use Spectrum PHY", spectrumPhy);
  cmd.Parse(argc, argv);

  WifiMcsSimulation sim;
  sim.Run(nStations, mcsValue, channelWidth, simulationTime, payloadSize, enableRtsCts,
          enableUlOfdma, enableDlMu, enableExtendedBlockAck, targetDataRate, useTcp,
          distance, guardInterval, band, spectrumPhy);

  return 0;
}