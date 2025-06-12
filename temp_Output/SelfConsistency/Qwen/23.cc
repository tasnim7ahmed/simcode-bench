#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputTest");

class Wifi7ThroughputExperiment
{
public:
  Wifi7ThroughputExperiment();
  void Run(uint8_t mcs, uint16_t channelWidth, bool shortGi,
           double frequency, bool enableOfdma, bool enableBsrp,
           uint32_t nStations, std::string trafficType, uint32_t payloadSize,
           uint32_t mpduBufferSize, bool auxPhy);
  void LogResults();

private:
  double CalculateThroughput(Ptr<FlowMonitor> flowMonitor, FlowId flowId);

  uint8_t m_mcs;
  uint16_t m_channelWidth;
  bool m_shortGi;
  double m_frequency;
  bool m_enableOfdma;
  bool m_enableBsrp;
  uint32_t m_nStations;
  std::string m_trafficType;
  uint32_t m_payloadSize;
  uint32_t m_mpduBufferSize;
  bool m_auxPhy;
  double m_throughputMbps;
};

Wifi7ThroughputExperiment::Wifi7ThroughputExperiment()
    : m_mcs(0),
      m_channelWidth(20),
      m_shortGi(false),
      m_frequency(5.0),
      m_enableOfdma(false),
      m_enableBsrp(false),
      m_nStations(1),
      m_trafficType("udp"),
      m_payloadSize(1472),
      m_mpduBufferSize(1024),
      m_auxPhy(false),
      m_throughputMbps(0.0)
{
}

void
Wifi7ThroughputExperiment::Run(uint8_t mcs, uint16_t channelWidth, bool shortGi,
                               double frequency, bool enableOfdma, bool enableBsrp,
                               uint32_t nStations, std::string trafficType, uint32_t payloadSize,
                               uint32_t mpduBufferSize, bool auxPhy)
{
  m_mcs = mcs;
  m_channelWidth = channelWidth;
  m_shortGi = shortGi;
  m_frequency = frequency;
  m_enableOfdma = enableOfdma;
  m_enableBsrp = enableBsrp;
  m_nStations = nStations;
  m_trafficType = trafficType;
  m_payloadSize = payloadSize;
  m_mpduBufferSize = mpduBufferSize;
  m_auxPhy = auxPhy;

  NodeContainer wifiApNode;
  wifiApNode.Create(1);
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nStations);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211be);

  WifiMacHelper macHelper;
  Ssid ssid = Ssid("wifi7-throughput-test");

  macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true));
  if (m_enableOfdma)
  {
    macHelper.SetAttribute("UlOfdmaEnabled", BooleanValue(true));
  }
  if (m_enableBsrp)
  {
    macHelper.SetAttribute("BsrpEnabled", BooleanValue(true));
  }

  phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));
  phyHelper.Set("ShortGuardInterval", BooleanValue(shortGi));
  phyHelper.Set("Frequency", DoubleValue(frequency * 1e9));

  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install(phyHelper, macHelper, wifiApNode);

  macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (trafficType == "tcp")
  {
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    for (uint32_t i = 0; i < nStations; ++i)
    {
      serverApps.Add(sinkHelper.Install(wifiStaNodes.Get(i)));
    }

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), 9));
    onoff.SetConstantRate(DataRate("1000Mb/s"), payloadSize);
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientApps.Add(onoff.Install(wifiApNode.Get(0)));
  }
  else // UDP
  {
    UdpServerHelper server;
    serverApps.Add(server.Install(wifiStaNodes));

    UdpClientHelper client(staInterfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientApps.Add(client.Install(wifiApNode.Get(0)));
  }

  serverApps.Start(Seconds(1.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> flowMonitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  FlowId flowId = 1;
  m_throughputMbps = CalculateThroughput(flowMonitor, flowId);

  Simulator::Destroy();
}

double
Wifi7ThroughputExperiment::CalculateThroughput(Ptr<FlowMonitor> flowMonitor, FlowId flowId)
{
  FlowMonitor::FlowStats stats = flowMonitor->GetFlowStats().find(flowId)->second;
  double throughput = (stats.rxBytes * 8) / (1e6 * (stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds()));
  return throughput;
}

void
Wifi7ThroughputExperiment::LogResults()
{
  std::ostringstream oss;
  oss << "MCS: " << static_cast<uint32_t>(m_mcs)
      << ", Channel Width: " << m_channelWidth << " MHz"
      << ", GI: " << (m_shortGi ? "Short" : "Long")
      << ", Frequency: " << m_frequency << " GHz"
      << ", Traffic Type: " << m_trafficType
      << ", Throughput: " << m_throughputMbps << " Mbps";

  NS_LOG_UNCOND(oss.str());
  std::cout << oss.str() << std::endl;
}

int
main(int argc, char* argv[])
{
  uint8_t mcs = 11;
  uint16_t channelWidth = 160;
  bool shortGi = true;
  double frequency = 5.0;
  bool enableOfdma = true;
  bool enableBsrp = true;
  uint32_t nStations = 4;
  std::string trafficType = "udp";
  uint32_t payloadSize = 1472;
  uint32_t mpduBufferSize = 1024;
  bool auxPhy = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("mcs", "MCS value", mcs);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGi", "Use short guard interval", shortGi);
  cmd.AddValue("frequency", "Operating frequency in GHz (2.4, 5, or 6)", frequency);
  cmd.AddValue("enableOfdma", "Enable UL OFDMA", enableOfdma);
  cmd.AddValue("enableBsrp", "Enable BSRP", enableBsrp);
  cmd.AddValue("nStations", "Number of stations", nStations);
  cmd.AddValue("trafficType", "Traffic type (tcp or udp)", trafficType);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
  cmd.AddValue("auxPhy", "Enable auxiliary PHY", auxPhy);
  cmd.Parse(argc, argv);

  if (frequency != 2.4 && frequency != 5.0 && frequency != 6.0)
  {
    NS_FATAL_ERROR("Invalid frequency specified. Choose from 2.4, 5, or 6 GHz.");
  }

  if (trafficType != "tcp" && trafficType != "udp")
  {
    NS_FATAL_ERROR("Invalid traffic type. Use 'tcp' or 'udp'.");
  }

  Wifi7ThroughputExperiment experiment;
  experiment.Run(mcs, channelWidth, shortGi, frequency, enableOfdma, enableBsrp,
                 nStations, trafficType, payloadSize, mpduBufferSize, auxPhy);
  experiment.LogResults();

  return 0;
}