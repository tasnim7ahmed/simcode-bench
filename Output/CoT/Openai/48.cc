#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi11nPhyEval");

enum PhyModelType { PHY_YANS, PHY_SPECTRUM };

struct SimulationConfig
{
  double simulationTime;
  double distance;
  std::vector<uint8_t> mcsIndices;
  std::vector<uint16_t> channelWidths;
  std::vector<bool> shortGuardIntervals;
  bool useTcp;
  bool enablePcap;
  PhyModelType phyModel;
};

struct MeasurementResults
{
  uint8_t mcsIdx;
  uint16_t channelWidth;
  bool shortGi;
  double throughputMbps;
  double avgSnr;
  double avgRssi;
  double avgNoiseDbm;
};

class PhyStatsCollector : public Object
{
public:
  PhyStatsCollector () : totalRx (0), totalSnr (0), totalRssi (0), totalNoise (0), count (0) {}

  void RxCallback (Ptr<const Packet> p, double snr, WifiTxVector txvector, MpduInfo a)
  {
    totalRx++;
    totalSnr += snr;
    if (a.rxInfo) // only for SpectrumWifiPhy
      {
        totalRssi += a.rxInfo->signalDbm;
        totalNoise += a.rxInfo->noiseDbm;
      }
    else
      {
        // use default values if no detailed info
        totalRssi += 0;
        totalNoise += 0;
      }
    count++;
  }

  void MonitorSnifferRx (Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo mpduInfo, SignalNoiseDbm signalNoise)
  {
    totalRssi += signalNoise.signal;
    totalNoise += signalNoise.noise;
    // SNR in dB, approximate
    totalSnr += (signalNoise.signal - signalNoise.noise);
    count++;
  }

  void Reset ()
  {
    totalRx = 0;
    totalSnr = 0;
    totalRssi = 0;
    totalNoise = 0;
    count = 0;
  }

  double GetAvgSnr () const
  {
    return (count > 0) ? totalSnr / count : 0.0;
  }
  double GetAvgRssi () const
  {
    return (count > 0) ? totalRssi / count : 0.0;
  }
  double GetAvgNoise () const
  {
    return (count > 0) ? totalNoise / count : 0.0;
  }
private:
  uint64_t totalRx;
  double totalSnr;
  double totalRssi;
  double totalNoise;
  uint64_t count;
};

void RunSimulation (const SimulationConfig& cfg, std::vector<MeasurementResults>& results)
{
  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (1);
  wifiApNode.Create (1);

  YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default ();
  Ptr<WifiPhy> phy;
  Ptr<SpectrumWifiPhy> spectrumPhy;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);
  WifiMacHelper mac;
  NetDeviceContainer staDevice, apDevice;

  if (cfg.phyModel == PHY_YANS)
    {
      YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
      phyHelper.SetChannel (yansChannel.Create ());
      phyHelper.Set ("ShortGuardEnabled", BooleanValue (false)); // We'll set it later
      phy = phyHelper.Create (wifiStaNodes.Get (0), wifiApNode.Get (0));
      phyHelper.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (Ssid ("WifiTest")));
      staDevice = wifi.Install (phyHelper, mac, wifiStaNodes);

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (Ssid ("WifiTest")));
      apDevice = wifi.Install (phyHelper, mac, wifiApNode);
    }
  else // PHY_SPECTRUM
    {
      SpectrumWifiPhyHelper spectrumPhyHelper = SpectrumWifiPhyHelper::Default ();
      spectrumPhyHelper.SetChannel (YansWifiChannelHelper::Default ().Create ());
      spectrumPhyHelper.Set ("ShortGuardEnabled", BooleanValue (false));
      spectrumPhy = spectrumPhyHelper.Create (wifiStaNodes.Get (0), wifiApNode.Get (0));
      spectrumPhyHelper.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (Ssid ("WifiTest")));
      staDevice = wifi.Install (spectrumPhyHelper, mac, wifiStaNodes);

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (Ssid ("WifiTest")));
      apDevice = wifi.Install (spectrumPhyHelper, mac, wifiApNode);
    }

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
  positionAlloc->Add (Vector (cfg.distance, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  uint16_t port = 9;
  ApplicationContainer sinkApp, sourceApp;

  // Use TCP or UDP depending on cfg
  if (cfg.useTcp)
    {
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
      sinkApp = sinkHelper.Install (wifiApNode.Get (0));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (cfg.simulationTime + 1));

      OnOffHelper client ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), port)));
      client.SetAttribute ("DataRate", DataRateValue (DataRate ("200Mbps")));
      client.SetAttribute ("PacketSize", UintegerValue (1472));
      client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      client.SetAttribute ("StopTime", TimeValue (Seconds (cfg.simulationTime)));
      sourceApp = client.Install (wifiStaNodes.Get (0));
    }
  else
    {
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
      sinkApp = sinkHelper.Install (wifiApNode.Get (0));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (cfg.simulationTime + 1));

      OnOffHelper client ("ns3::UdpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), port)));
      client.SetAttribute ("DataRate", DataRateValue (DataRate ("200Mbps")));
      client.SetAttribute ("PacketSize", UintegerValue (1472));
      client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      client.SetAttribute ("StopTime", TimeValue (Seconds (cfg.simulationTime)));
      sourceApp = client.Install (wifiStaNodes.Get (0));
    }

  PhyStatsCollector stats;
  // Connect trace sources
  if (cfg.phyModel == PHY_YANS)
    {
      Config::ConnectWithoutContext ("/NodeList/0/DeviceList/0/Phy/MonitorSnifferRx",
              MakeCallback (&PhyStatsCollector::MonitorSnifferRx, &stats));
    }
  else
    {
      Config::ConnectWithoutContext ("/NodeList/0/DeviceList/0/Phy/RxSignalToNoise",
              MakeCallback (&PhyStatsCollector::RxCallback, &stats));
    }

  if (cfg.enablePcap)
    {
      if (cfg.phyModel == PHY_YANS)
        {
          YansWifiPhyHelper::Default ().EnablePcapAll ("wifi-11n-yans");
        }
      else
        {
          SpectrumWifiPhyHelper::Default ().EnablePcapAll ("wifi-11n-spectrum");
        }
    }

  Simulator::Stop (Seconds (cfg.simulationTime + 1));

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();
  Simulator::Run ();

  double throughput = 0.0;
  if (cfg.useTcp)
    {
      Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApp.Get (0));
      if (sink1)
        {
          uint64_t rxBytes = sink1->GetTotalRx ();
          throughput = (rxBytes * 8.0) / (cfg.simulationTime * 1e6); // Mbps
        }
    }
  else
    {
      Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApp.Get (0));
      if (sink1)
        {
          uint64_t rxBytes = sink1->GetTotalRx ();
          throughput = (rxBytes * 8.0) / (cfg.simulationTime * 1e6);
        }
    }

  MeasurementResults r;
  r.throughputMbps = throughput;
  r.mcsIdx = wifiStaNodes.Get (0)->GetDevice (0)->GetObject<WifiNetDevice> ()->GetPhy ()->GetMcs ();
  r.channelWidth = wifiStaNodes.Get (0)->GetDevice (0)->GetObject<WifiNetDevice> ()->GetPhy ()->GetChannelWidth ();
  r.shortGi = wifiStaNodes.Get (0)->GetDevice (0)->GetObject<WifiNetDevice> ()->GetPhy ()->GetShortGuardEnabled ();
  r.avgSnr = stats.GetAvgSnr ();
  r.avgRssi = stats.GetAvgRssi ();
  r.avgNoiseDbm = stats.GetAvgNoise ();

  results.push_back (r);

  Simulator::Destroy ();
}

int main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  double distance = 10.0;
  std::string phyModelStr = "YANS";
  bool useTcp = false;
  bool enablePcap = false;
  std::string mcsList = "0,1,2,3,4,5,6,7";
  std::string channelWidthList = "20,40";
  std::string giList = "0,1"; // 0: normal, 1: short

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between AP and STA in meters", distance);
  cmd.AddValue ("phyModel", "PHY model: YANS or SPECTRUM", phyModelStr);
  cmd.AddValue ("tcp", "Use TCP traffic", useTcp);
  cmd.AddValue ("pcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("mcsList", "Comma-separated MCS indices", mcsList);
  cmd.AddValue ("channelWidthList", "Comma-separated Channel widths (MHz)", channelWidthList);
  cmd.AddValue ("giList", "Comma-separated Guard Interval flags (0=normal,1=short)", giList);
  cmd.Parse (argc, argv);

  std::vector<uint8_t> mcsIndices;
  std::stringstream mcsSS (mcsList);
  std::string mcsToken;
  while (std::getline (mcsSS, mcsToken, ','))
    mcsIndices.push_back (std::stoi (mcsToken));

  std::vector<uint16_t> channelWidths;
  std::stringstream chSS (channelWidthList);
  std::string chToken;
  while (std::getline (chSS, chToken, ','))
    channelWidths.push_back (std::stoi (chToken));

  std::vector<bool> shortGuardIntervals;
  std::stringstream giSS (giList);
  std::string giToken;
  while (std::getline (giSS, giToken, ','))
    shortGuardIntervals.push_back (std::stoi (giToken) != 0);

  PhyModelType modelType = (phyModelStr == "SPECTRUM") ? PHY_SPECTRUM : PHY_YANS;

  SimulationConfig cfg;
  cfg.simulationTime = simulationTime;
  cfg.distance = distance;
  cfg.mcsIndices = mcsIndices;
  cfg.channelWidths = channelWidths;
  cfg.shortGuardIntervals = shortGuardIntervals;
  cfg.useTcp = useTcp;
  cfg.enablePcap = enablePcap;
  cfg.phyModel = modelType;

  std::vector<MeasurementResults> results;

  for (uint16_t cw : channelWidths)
    {
      for (auto sgi : shortGuardIntervals)
        {
          for (auto mcs : mcsIndices)
            {
              Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
              Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("HtMcs0"));
              Config::SetDefault ("ns3::WifiRemoteStationManager::DataMode", StringValue ("HtMcs" + std::to_string (mcs)));
              Config::SetDefault ("ns3::WifiRemoteStationManager::ControlMode", StringValue ("HtMcs0"));
              Config::SetDefault ("ns3::WifiPhy::ChannelWidth", UintegerValue (cw));
              Config::SetDefault ("ns3::WifiPhy::ShortGuardEnabled", BooleanValue (sgi));
              std::cout << "Running: MCS=" << int(mcs) << " ChannelWidth=" << cw << "MHz ShortGI=" << sgi << std::endl;

              RunSimulation (cfg, results);
            }
        }
    }

  std::cout << std::endl
            << "Results: Throughput (Mbps), Avg SNR (dB), Avg RSSI (dBm), Avg Noise (dBm)" << std::endl;
  std::cout << "MCS,ChannelWidth,ShortGI,Throughput,AvgSNR,AvgRSSI,AvgNoise" << std::endl;
  for (const auto& r : results)
    {
      std::cout << int(r.mcsIdx) << ","
                << r.channelWidth << ","
                << (r.shortGi?"1":"0") << ","
                << std::fixed << std::setprecision(3)
                << r.throughputMbps << ","
                << r.avgSnr << ","
                << r.avgRssi << ","
                << r.avgNoiseDbm << std::endl;
    }
  std::cout << std::endl
           << "To switch between UDP and TCP, use --tcp=0 or --tcp=1." << std::endl
           << "To enable PCAP packet capture, use --pcap=1." << std::endl
           << "You can set model with --phyModel=YANS or --phyModel=SPECTRUM." << std::endl
           << "Change MCS/channel width/guard interval using --mcsList, --channelWidthList, --giList." << std::endl;

  return 0;
}