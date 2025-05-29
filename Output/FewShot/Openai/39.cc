#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifiMinstrelSim");

struct Stats
{
  double distance;
  double throughput;
};

class ThroughputTracer
{
public:
  ThroughputTracer(Ptr<PacketSink> sink, double interval, double distance, std::vector<Stats>* results)
    : m_sink(sink), m_interval(interval), m_lastTotalRx(0), m_distance(distance), m_results(results)
  {
    Simulator::Schedule(Seconds(m_interval), &ThroughputTracer::Trace, this);
  }

  void Trace()
  {
    double cur = Simulator::Now().GetSeconds();
    uint64_t totalRx = m_sink->GetTotalRx();
    double throughput = (totalRx - m_lastTotalRx) * 8.0 / (m_interval * 1e6); // Mbps
    m_lastTotalRx = totalRx;
    Stats s;
    s.distance = m_distance;
    s.throughput = throughput;
    m_results->push_back(s);
  }
private:
  Ptr<PacketSink> m_sink;
  double m_interval;
  uint64_t m_lastTotalRx;
  double m_distance;
  std::vector<Stats>* m_results;
};

static void
StaRateChanged(std::string context,
               uint32_t mode,
               uint32_t rate,
               Mac48Address remote)
{
  NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() <<
              " STA changed rate: mode=" << mode << " rate=" << rate << " remote=" << remote);
}

int main(int argc, char *argv[])
{
  double initialDistance = 1.0; // meters
  double maxDistance = 100.0; // meters
  double distanceStep = 5.0; // meters
  double totalTimePerStep = 2.0; // seconds
  uint32_t payloadSize = 1472; // bytes
  std::string phyMode = "HtMcs7"; // 802.11n MCS7
  std::string output = "throughput-vs-distance.plt";
  bool enableLogging = false;

  CommandLine cmd;
  cmd.AddValue("initialDistance", "Initial distance between AP and STA (meters)", initialDistance);
  cmd.AddValue("maxDistance", "Maximum distance between AP and STA (meters)", maxDistance);
  cmd.AddValue("distanceStep", "Distance step size (meters)", distanceStep);
  cmd.AddValue("timeStep", "Time interval per distance step (seconds)", totalTimePerStep);
  cmd.AddValue("log", "Enable rate log", enableLogging);
  cmd.AddValue("output", "Gnuplot output file", output);
  cmd.Parse(argc, argv);

  if (enableLogging)
    {
      LogComponentEnable("RateAdaptiveWifiMinstrelSim", LOG_LEVEL_INFO);
      LogComponentEnable("MinstrelWifiManager", LOG_LEVEL_INFO);
    }

  std::vector<Stats> results;

  for (double distance = initialDistance; distance <= maxDistance; distance += distanceStep)
    {
      NodeContainer wifiStaNode;
      wifiStaNode.Create(1);
      NodeContainer wifiApNode;
      wifiApNode.Create(1);

      YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
      phy.SetChannel(channel.Create());

      WifiHelper wifi;
      wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

      // Configure AP (default rate manager)
      WifiMacHelper mac;
      Ssid ssid = Ssid("wifi-adapt");
      mac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                   "DataMode", StringValue(phyMode),
                                   "ControlMode", StringValue(phyMode));

      NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

      // Configure STA Minstrel
      WifiHelper wifiSta;
      wifiSta.SetStandard(WIFI_STANDARD_80211n_5GHZ);
      WifiMacHelper macSta;
      macSta.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "ActiveProbing", BooleanValue(false));
      wifiSta.SetRemoteStationManager("ns3::MinstrelWifiManager");
      NetDeviceContainer staDevice = wifiSta.Install(phy, macSta, wifiStaNode);

      // Mobility setup
      MobilityHelper mobility;

      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
      positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP

      positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA

      mobility.SetPositionAllocator(positionAlloc);

      mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      mobility.Install(wifiApNode);
      mobility.Install(wifiStaNode);

      // Internet stack
      InternetStackHelper stack;
      stack.Install(wifiApNode);
      stack.Install(wifiStaNode);

      Ipv4AddressHelper address;
      address.SetBase("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer staInterfaces;
      Ipv4InterfaceContainer apInterfaces;
      NetDeviceContainer allDevs;
      allDevs.Add(apDevice);
      allDevs.Add(staDevice);
      Ipv4InterfaceContainer interfaces = address.Assign(allDevs);

      // UDP traffic: AP -> STA (AP client, STA server)
      uint16_t port = 5000;
      Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
      PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
      ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNode.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(totalTimePerStep));

      // CBR traffic
      OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      onoff.SetAttribute("DataRate", StringValue("400Mbps"));
      onoff.SetAttribute("StartTime", TimeValue(Seconds(0.1)));
      onoff.SetAttribute("StopTime", TimeValue(Seconds(totalTimePerStep)));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer apps = onoff.Install(wifiApNode.Get(0));

      // STA rate change logging
      if (enableLogging)
        {
          Config::Connect("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RateChange",
                          MakeCallback(&StaRateChanged));
        }

      // Trace throughput
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
      ThroughputTracer tputTracer(sink, totalTimePerStep, distance, &results);

      Simulator::Stop(Seconds(totalTimePerStep + 0.1));
      Simulator::Run();
      Simulator::Destroy();
    }

  // Output Gnuplot file
  Gnuplot plot(output, "Throughput vs Distance (Minstrel at STA)");
  plot.SetTerminal("png");
  plot.SetTitle("Throughput vs Distance (Minstrel at STA)");
  plot.SetLegend("Distance (m)", "Throughput (Mbps)");

  Gnuplot2dDataset dataset;
  dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  for (const auto& s : results)
    {
      dataset.Add(s.distance, s.throughput);
    }
  plot.AddDataset(dataset);

  std::ofstream ofs(output);
  plot.GenerateOutput(ofs);
  ofs.close();

  return 0;
}