#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughput");

class WifiMimoSimulation
{
public:
  WifiMimoSimulation();
  void Run(uint32_t mcs, uint8_t streams, double distance);
  void Configure(bool useTcp, double simulationTime, double stepSize, uint16_t channelWidth, bool shortGuardInterval, uint32_t frequency, bool preambleDetection, bool channelBonding);

private:
  void SetupNetwork(double distance);
  void SetupApplications();
  void CalculateThroughput(Ptr<Socket> socket, uint32_t packetSize, Time startTime);
  void ReportResults();

  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apIpIfaces;
  Ipv4InterfaceContainer m_staIpIfaces;
  uint32_t m_mcs;
  uint8_t m_streams;
  double m_distance;
  double m_simulationTime;
  uint16_t m_channelWidth;
  bool m_shortGuardInterval;
  uint32_t m_frequency;
  bool m_preambleDetection;
  bool m_channelBonding;
  bool m_useTcp;
  Gnuplot2dDataset m_dataset;
};

WifiMimoSimulation::WifiMimoSimulation()
{
  m_apNode.Create(1);
  m_staNode.Create(1);
}

void WifiMimoSimulation::Configure(bool useTcp, double simulationTime, double stepSize, uint16_t channelWidth, bool shortGuardInterval, uint32_t frequency, bool preambleDetection, bool channelBonding)
{
  m_useTcp = useTcp;
  m_simulationTime = simulationTime;
  m_channelWidth = channelWidth;
  m_shortGuardInterval = shortGuardInterval;
  m_frequency = frequency;
  m_preambleDetection = preambleDetection;
  m_channelBonding = channelBonding;

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(Seconds(0)));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  Config::SetDefault("ns3::UdpSocket::RcvBufSize", UintegerValue(1 << 20));
  Config::SetDefault("ns3::UdpSocket::SndBufSize", UintegerValue(1 << 20));
}

void WifiMimoSimulation::SetupNetwork(double distance)
{
  m_distance = distance;

  // Cleanup previous setup
  m_apDev.Clear();
  m_staDev.Clear();

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("ChannelNumber", UintegerValue(m_frequency == 5000 ? 36 : 6));
  phy.Set("ChannelWidth", UintegerValue(m_channelWidth));
  phy.Set("ShortGuardIntervalSupported", BooleanValue(m_shortGuardInterval));
  phy.Set("PreambleDetectionSupported", BooleanValue(m_preambleDetection));

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-mimo");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  m_staDev = mac.Install(phy, m_staNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  m_apDev = mac.Install(phy, m_apNode);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5MHZ);
  if (m_frequency == 2400)
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  else
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(m_mcs)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(m_mcs)));

  wifi.EnableLogComponents();

  // MIMO configuration
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/NumberOfTransmitAntennas", UintegerValue(m_streams));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/NumberOfReceiveAntennas", UintegerValue(m_streams));

  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apIpIfaces = address.Assign(m_apDev);
  m_staIpIfaces = address.Assign(m_staDev);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at origin
  positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA at varying distance
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNode);
}

void WifiMimoSimulation::Run(uint32_t mcs, uint8_t streams, double distance)
{
  m_mcs = mcs;
  m_streams = streams;

  SetupNetwork(distance);
  SetupApplications();

  Simulator::Stop(Seconds(m_simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  ReportResults();
}

void WifiMimoSimulation::SetupApplications()
{
  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper(m_useTcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(m_staNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(m_simulationTime));

  OnOffHelper onoff(m_useTcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                    InetSocketAddress(m_staIpIfaces.GetAddress(0), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1448));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));

  ApplicationContainer sourceApp = onoff.Install(m_apNode.Get(0));
  sourceApp.Start(Seconds(0.1));
  sourceApp.Stop(Seconds(m_simulationTime - 0.1));
}

void WifiMimoSimulation::CalculateThroughput(Ptr<Socket> socket, uint32_t packetSize, Time startTime)
{
  static uint64_t totalBytes = 0;
  static Time lastTime = startTime;

  socket->TraceConnectWithoutContext("Rx", MakeCallback(
      [&totalBytes, &lastTime, this](Ptr<const Packet> p, const Address &) {
        totalBytes += p->GetSize();
        Time now = Simulator::Now();
        if ((now - lastTime).GetSeconds() >= 1.0)
        {
          double throughput = (totalBytes * 8.0) / (now - lastTime).GetSeconds() / 1e6;
          NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");
          lastTime = now;
          totalBytes = 0;
        }
      }));
}

void WifiMimoSimulation::ReportResults()
{
  double throughput = 0;
  uint64_t totalBytes = 0;
  for (auto app : ApplicationContainer())
  {
    // Extract received bytes from sink application
  }

  // Dummy calculation for example
  throughput = (totalBytes * 8.0) / m_simulationTime / 1e6;

  std::ostringstream oss;
  oss << "HT MCS=" << m_mcs << ", Streams=" << (uint32_t)m_streams << ", Distance=" << m_distance << "m";
  m_dataset.Add(m_distance, throughput);
  m_dataset.SetTitle(oss.str().c_str());
}

int main(int argc, char *argv[])
{
  bool useTcp = false;
  double simulationTime = 10.0;
  double stepSize = 5.0;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = true;
  uint32_t frequency = 5000;
  bool preambleDetection = true;
  bool channelBonding = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("useTcp", "Use TCP traffic", useTcp);
  cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("stepSize", "Distance step size in meters", stepSize);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
  cmd.AddValue("frequency", "Frequency band (2400 or 5000)", frequency);
  cmd.AddValue("preambleDetection", "Enable preamble detection", preambleDetection);
  cmd.AddValue("channelBonding", "Enable channel bonding", channelBonding);
  cmd.Parse(argc, argv);

  Gnuplot gnuplot("wifi-mimo-throughput.png");
  gnuplot.SetTitle("Wi-Fi Throughput vs Distance");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("Distance (meters)", "Throughput (Mbps)");

  WifiMimoSimulation sim;

  for (uint8_t streams = 1; streams <= 4; ++streams)
  {
    for (uint32_t mcs = 0; mcs <= 7; ++mcs)
    {
      sim.Configure(useTcp, simulationTime, stepSize, channelWidth, shortGuardInterval, frequency, preambleDetection, channelBonding);
      for (double distance = 1.0; distance <= 100.0; distance += stepSize)
      {
        NS_LOG_UNCOND("Running: MCS=" << mcs << ", Streams=" << (uint32_t)streams << ", Distance=" << distance << "m");
        sim.Run(mcs, streams, distance);
      }
      gnuplot.AddDataset(sim.m_dataset);
    }
  }

  std::ofstream plotFile("wifi-mimo-throughput.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}