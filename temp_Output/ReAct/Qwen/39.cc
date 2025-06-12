#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifi");

class ThroughputStats {
public:
  void TraceRx(Ptr<const Packet> packet, const Address &) {
    m_total += packet->GetSize();
  }

  void Report(double distance) {
    double now = Simulator::Now().GetSeconds();
    if (m_lastTime > 0 && now > m_lastTime) {
      double throughput = (m_total - m_lastTotal) * 8.0 / (now - m_lastTime) / 1e6;
      std::cout << "Distance: " << distance << " m, Throughput: " << throughput << " Mbps" << std::endl;
      m_gnuPlot.Add(dist, throughput);
    }
    m_lastTotal = m_total;
    m_lastTime = now;
  }

  static Gnuplot2dDataset m_gnuPlot;
  static double m_lastTime;
  static uint64_t m_total;
  static uint64_t m_lastTotal;
  static double dist;
};

Gnuplot2dDataset ThroughputStats::m_gnuPlot;
double ThroughputStats::m_lastTime = 0;
uint64_t ThroughputStats::m_total = 0;
uint64_t ThroughputStats::m_lastTotal = 0;
double ThroughputStats::dist = 0;

void PrintRate(uint32_t rate) {
  NS_LOG_UNCOND("STA Rate changed to " << rate << " bps");
}

int main(int argc, char *argv[]) {
  uint32_t steps = 5;
  Time stepInterval = Seconds(1);
  double initialDistance = 1.0;
  double distanceStep = 2.0;
  bool loggingEnabled = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("steps", "Number of distance steps", steps);
  cmd.AddValue("step-interval", "Time between each distance step", stepInterval);
  cmd.AddValue("initial-distance", "Initial distance between AP and STA", initialDistance);
  cmd.AddValue("distance-step", "Increment in distance at each step", distanceStep);
  cmd.AddValue("logging", "Enable logging output for rate changes", loggingEnabled);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("rate-adaptive")),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("rate-adaptive")));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(initialDistance),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distanceStep),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  // Moving STA using ConstantVelocityMobilityModel
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNode);
  wifiStaNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0.0, 0.0, 0.0));

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Simulator::StopTime());

  ThroughputStats stats;
  Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&ThroughputStats::TraceRx));

  OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("400Mbps")));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer sourceApp = onoff.Install(wifiApNode.Get(0));
  sourceApp.Start(Seconds(0.0));
  sourceApp.Stop(Simulator::StopTime());

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("rate-adaptive-wifi.tr");
  phy.EnableAsciiAll(stream);

  Gnuplot gnuplot("rate-adaptive-wifi-throughput-vs-distance.plt");
  gnuplot.SetTitle("Throughput vs Distance");
  gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");
  gnuplot.AddDataset(stats.m_gnuPlot);

  if (loggingEnabled) {
    Config::Connect("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/DataMode", MakeCallback(&PrintRate));
  }

  Time nextReport = stepInterval;
  for (uint32_t i = 0; i < steps; ++i) {
    Simulator::Schedule(nextReport, &MobilityModel::SetPosition, wifiApNode.Get(0)->GetObject<MobilityModel>(), Vector(initialDistance + distanceStep * (i + 1), 0.0, 0.0));
    Simulator::Schedule(nextReport, &ThroughputStats::Report, &stats, initialDistance + distanceStep * (i + 1));
    nextReport += stepInterval;
  }

  Simulator::Run();
  Simulator::Destroy();

  std::ofstream plotFile("throughput-vs-distance.gnu");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}