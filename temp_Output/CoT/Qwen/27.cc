#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsGoodput");

class WifiGoodputExperiment
{
public:
  WifiGoodputExperiment();
  void Setup(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval, Time simulationTime,
             std::string protocol, bool enableRtsCts, double distance, double expectedThroughput);

private:
  void RunSimulation();
  void CalculateThroughput();

  uint8_t m_mcs;
  uint16_t m_channelWidth;
  bool m_shortGuardInterval;
  Time m_simulationTime;
  std::string m_protocol;
  bool m_enableRtsCts;
  double m_distance;
  double m_expectedThroughput;
  uint16_t m_packetSize;
  uint32_t m_maxBytes;
};

WifiGoodputExperiment::WifiGoodputExperiment()
    : m_mcs(0), m_channelWidth(20), m_shortGuardInterval(true), m_simulationTime(Seconds(10)),
      m_protocol("udp"), m_enableRtsCts(false), m_distance(1.0), m_expectedThroughput(0),
      m_packetSize(1472), m_maxBytes(0)
{
}

void
WifiGoodputExperiment::Setup(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval,
                             Time simulationTime, std::string protocol, bool enableRtsCts,
                             double distance, double expectedThroughput)
{
  m_mcs = mcs;
  m_channelWidth = channelWidth;
  m_shortGuardInterval = shortGuardInterval;
  m_simulationTime = simulationTime;
  m_protocol = protocol;
  m_enableRtsCts = enableRtsCts;
  m_distance = distance;
  m_expectedThroughput = expectedThroughput;

  m_maxBytes = static_cast<uint32_t>(m_expectedThroughput * m_simulationTime.GetSeconds());
}

void
WifiGoodputExperiment::RunSimulation()
{
  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;

  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211n);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HTMCS" + std::to_string(m_mcs)),
                               "ControlMode", StringValue("HTMCS0"));

  if (m_shortGuardInterval)
    {
      Config::SetDefault("ns3::WifiNetDevice::ShortGuardIntervalSupported", BooleanValue(true));
    }

  phy.Set("ChannelWidth", UintegerValue(m_channelWidth));

  Ssid ssid = Ssid("wifi-goodput");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at origin
  positionAlloc->Add(Vector(m_distance, 0.0, 0.0)); // STA at distance

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  Ipv4InterfaceContainer staInterface;

  apInterface = address.Assign(apDevices);
  staInterface = address.Assign(staDevices);

  if (m_enableRtsCts)
    {
      Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(1));
    }

  ApplicationContainer serverApp;
  ApplicationContainer clientApp;

  if (m_protocol == "udp")
    {
      UdpServerHelper server(9);
      serverApp = server.Install(wifiStaNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(m_simulationTime);

      UdpClientHelper client(staInterface.GetAddress(0), 9);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(m_packetSize));

      clientApp = client.Install(wifiApNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(m_simulationTime - Seconds(0.1));
    }
  else if (m_protocol == "tcp")
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
      serverApp = sink.Install(wifiStaNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(m_simulationTime);

      OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), 9));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      client.SetAttribute("DataRate", DataRateValue(DataRate(m_expectedThroughput)));
      client.SetAttribute("PacketSize", UintegerValue(m_packetSize));
      client.SetAttribute("MaxBytes", UintegerValue(m_maxBytes));

      clientApp = client.Install(wifiApNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(m_simulationTime - Seconds(0.1));
    }

  Simulator::Stop(m_simulationTime);
  Simulator::Run();

  CalculateThroughput();

  Simulator::Destroy();
}

void
WifiGoodputExperiment::CalculateThroughput()
{
  double throughput = 0.0;
  if (m_protocol == "udp")
    {
      Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
      uint64_t totalPackets = server->GetReceived();
      throughput = ((totalPackets * m_packetSize * 8) / (m_simulationTime.GetSeconds() * 1e6));
    }
  else if (m_protocol == "tcp")
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
      uint64_t totalBytes = sink->GetTotalRx();
      throughput = ((totalBytes * 8) / (m_simulationTime.GetSeconds() * 1e6));
    }

  NS_LOG_UNCOND("MCS: " << (uint32_t)m_mcs << ", Channel Width: " << m_channelWidth
                        << "MHz, Guard Interval: " << (m_shortGuardInterval ? "Short" : "Long")
                        << ", Protocol: " << m_protocol << ", Distance: " << m_distance << "m, "
                        << "Measured Throughput: " << throughput << " Mbps");
}

int
main(int argc, char* argv[])
{
  uint8_t mcs = 0;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = true;
  double distance = 1.0;
  Time simulationTime = Seconds(10);
  std::string protocol = "udp";
  bool enableRtsCts = false;
  double expectedThroughput = 1000000; // Default in bps

  CommandLine cmd(__FILE__);
  cmd.AddValue("mcs", "Modulation and Coding Scheme (0-7)", mcs);
  cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
  cmd.AddValue("distance", "Distance between AP and station (meters)", distance);
  cmd.AddValue("simulationTime", "Duration of the simulation", simulationTime);
  cmd.AddValue("protocol", "Transport protocol (udp/tcp)", protocol);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue("expectedThroughput", "Expected throughput in bps for TCP mode", expectedThroughput);
  cmd.Parse(argc, argv);

  if (mcs > 7)
    {
      NS_FATAL_ERROR("Invalid MCS value: must be between 0 and 7.");
    }

  if (channelWidth != 20 && channelWidth != 40)
    {
      NS_FATAL_ERROR("Invalid channel width: must be 20 or 40 MHz.");
    }

  WifiGoodputExperiment experiment;
  experiment.Setup(mcs, channelWidth, shortGuardInterval, simulationTime, protocol, enableRtsCts,
                   distance, expectedThroughput);
  experiment.RunSimulation();

  return 0;
}