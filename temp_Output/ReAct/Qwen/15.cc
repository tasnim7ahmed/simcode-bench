#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughput");

class WifiThroughputExperiment
{
public:
  WifiThroughputExperiment();
  void Run();
  void Configure(uint8_t mcs, uint8_t streams, double distance);
  void SendData();
  void CalculateThroughput();

private:
  NodeContainer m_ap;
  NodeContainer m_sta;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apIf;
  Ipv4InterfaceContainer m_staIf;
  uint16_t m_port;
  uint32_t m_pktSize;
  Time m_simTime;
  uint32_t m_stepSize;
  bool m_useTcp;
  bool m_use5Ghz;
  uint16_t m_channelWidth;
  bool m_shortGuardInterval;
  bool m_preambleDetection;
  bool m_channelBonding;
  Gnuplot2dDataset m_plotDataset;
  std::map<std::string, Gnuplot2dDataset> m_datasets;
};

WifiThroughputExperiment::WifiThroughputExperiment()
  : m_port(9),
    m_pktSize(1472),
    m_simTime(Seconds(10)),
    m_stepSize(5),
    m_useTcp(false),
    m_use5Ghz(true),
    m_channelWidth(40),
    m_shortGuardInterval(true),
    m_preambleDetection(true),
    m_channelBonding(true)
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("tcp", "Use TCP instead of UDP", m_useTcp);
  cmd.AddValue("freq5GHz", "Use 5 GHz band", m_use5Ghz);
  cmd.AddValue("channelWidth", "Channel width (MHz)", m_channelWidth);
  cmd.AddValue("sgi", "Enable short guard interval", m_shortGuardInterval);
  cmd.AddValue("preamble", "Enable preamble detection", m_preambleDetection);
  cmd.AddValue("bonding", "Enable channel bonding", m_channelBonding);
  cmd.AddValue("simTime", "Simulation time (s)", m_simTime);
  cmd.AddValue("stepSize", "Distance step size (m)", m_stepSize);
  cmd.Parse(Simulator::Get argc(), Simulator::Get argv());

  m_ap.Create(1);
  m_sta.Create(1);

  m_port = 9;
  m_pktSize = 1472;
}

void
WifiThroughputExperiment::Configure(uint8_t mcs, uint8_t streams, double distance)
{
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> wifiChannel = channel.Create();
  phy.SetChannel(wifiChannel);

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  std::ostringstream oss;
  oss << "HtMcs" << static_cast<uint32_t>(mcs);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(oss.str()),
                               "ControlMode", StringValue(oss.str()));

  phy.Set("ShortGuardInterval", BooleanValue(m_shortGuardInterval));
  phy.Set("PreambleDetectionModelEnabled", BooleanValue(m_preambleDetection));

  if (m_use5Ghz)
    {
      phy.Set("ChannelNumber", UintegerValue(36));
    }
  else
    {
      phy.Set("ChannelNumber", UintegerValue(6));
    }

  phy.Set("ChannelWidth", UintegerValue(m_channelWidth));
  phy.Set("Nss", UintegerValue(streams));

  if (m_channelBonding)
    {
      wifi.Set("ChannelBonding", BooleanValue(true));
    }

  mac.SetType("ns3::ApWifiMac");
  m_apDev = wifi.Install(phy, mac, m_ap);
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-mimo")));
  m_staDev = wifi.Install(phy, mac, m_sta);

  InternetStackHelper stack;
  stack.Install(m_ap);
  stack.Install(m_sta);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apIf = address.Assign(m_apDev);
  m_staIf = address.Assign(m_staDev);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_ap);
  mobility.Install(m_sta);
}

void
WifiThroughputExperiment::SendData()
{
  Address sinkAddress(InetSocketAddress(m_staIf.GetAddress(0), m_port));
  PacketSinkHelper packetSinkHelper(m_useTcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), m_port));
  ApplicationContainer sinkApp = packetSinkHelper.Install(m_sta.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(m_simTime);

  OnOffHelper onoff(m_useTcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                    sinkAddress);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(m_pktSize));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

  ApplicationContainer srcApp = onoff.Install(m_ap.Get(0));
  srcApp.Start(Seconds(1.0));
  srcApp.Stop(m_simTime - Seconds(1.0));
}

void
WifiThroughputExperiment::CalculateThroughput()
{
  double totalRxBytes = DynamicCast<PacketSink>(m_sinkApp.Get(0))->GetTotalRx();
  double throughput = (totalRxBytes * 8) / (m_simTime.GetSeconds() * 1e6);
  NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");
}

void
WifiThroughputExperiment::Run()
{
  for (uint8_t streams = 1; streams <= 4; ++streams)
    {
      for (uint8_t mcs = 0; mcs <= 7; ++mcs)
        {
          std::string datasetName = "HT MCS" + std::to_string(mcs) + " Streams" + std::to_string(streams);
          m_datasets[datasetName].SetTitle(datasetName);
          m_datasets[datasetName].SetStyle(Gnuplot2dDataset::LINES_POINTS);

          for (double distance = 10.0; distance <= 100.0; distance += m_stepSize)
            {
              RngSeedManager::SetRun(static_cast<uint64_t>(distance + streams * 100 + mcs * 1000));
              Configure(mcs, streams, distance);
              SendData();

              Simulator::Stop(m_simTime);
              Simulator::Run();

              double totalRxBytes = DynamicCast<PacketSink>(m_sinkApp.Get(0))->GetTotalRx();
              double throughput = (totalRxBytes * 8) / (m_simTime.GetSeconds() * 1e6);
              m_datasets[datasetName].Add(distance, throughput);

              Simulator::Destroy();
            }
        }
    }

  Gnuplot plot("throughput_vs_distance_mimo.png");
  plot.SetTitle("Throughput vs Distance for 802.11n MIMO");
  plot.SetTerminal("png");
  plot.SetLegend("Distance (meters)", "Throughput (Mbps)");

  for (auto& pair : m_datasets)
    {
      plot.AddDataset(pair.second);
    }

  std::ofstream plotFile("throughput_vs_distance_mimo.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char* argv[])
{
  WifiThroughputExperiment experiment;
  experiment.Run();
  return 0;
}