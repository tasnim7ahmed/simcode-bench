#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGoodputEvaluation");

class WifiGoodputExperiment
{
public:
  WifiGoodputExperiment();
  void Run(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval, double distance, 
           bool enableRtsCts, Time simulationTime, bool useTcp, DataRate expectedThroughput);

private:
  void SetupNetwork(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval, double distance,
                    bool enableRtsCts);
  void SetupApplications(bool useTcp, Time simulationTime);
  void OnTxPacket(Ptr<const Packet>);
  void OnRxPacket(Ptr<const Packet>);

  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterface;
  uint64_t m_txPackets;
  uint64_t m_rxPackets;
};

WifiGoodputExperiment::WifiGoodputExperiment()
  : m_txPackets(0), m_rxPackets(0)
{
}

void
WifiGoodputExperiment::Run(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval, double distance,
                           bool enableRtsCts, Time simulationTime, bool useTcp, DataRate expectedThroughput)
{
  SetupNetwork(mcs, channelWidth, shortGuardInterval, distance, enableRtsCts);
  SetupApplications(useTcp, simulationTime);

  Simulator::Stop(simulationTime);
  Simulator::Run();

  double durationSeconds = simulationTime.GetSeconds();
  double goodputMbps = (m_rxPackets * 8.0) / (durationSeconds * 1e6);
  std::cout << "MCS: " << (uint32_t)mcs
            << ", Channel Width: " << channelWidth << " MHz"
            << ", Guard Interval: " << (shortGuardInterval ? "Short" : "Long")
            << ", Distance: " << distance << " m"
            << ", RTS/CTS: " << (enableRtsCts ? "Enabled" : "Disabled")
            << ", Protocol: " << (useTcp ? "TCP" : "UDP")
            << ", Expected Throughput: " << expectedThroughput.GetBitRate() / 1e6 << " Mbps"
            << ", Measured Goodput: " << goodputMbps << " Mbps"
            << ", Packets RX: " << m_rxPackets
            << std::endl;

  Simulator::Destroy();
}

void
WifiGoodputExperiment::SetupNetwork(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval,
                                   double distance, bool enableRtsCts)
{
  m_apNode.Create(1);
  m_staNode.Create(1);

  YansWifiPhyHelper phy;
  WifiChannelHelper channel = WifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(std::string("HtMcs") + std::to_string(mcs)),
                               "ControlMode", StringValue(std::string("HtMcs") + std::to_string(mcs)));

  if (channelWidth == 40)
    {
      Config::SetDefault("ns3::WifiRemoteStationManager::ChannelWidthSet", UintegerValue(40));
    }

  if (shortGuardInterval)
    {
      Config::SetDefault("ns3::WifiRemoteStationManager::ShortGuardInterval", BooleanValue(true));
    }

  if (enableRtsCts)
    {
      mac.Set("RTSThreshold", UintegerValue(1));
    }

  Ssid ssid = Ssid("wifi-goodput");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  m_staDev = wifi.Install(phy, mac, m_staNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  m_apDev = wifi.Install(phy, mac, m_apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNode);

  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterface = address.Assign(m_apDev);
  m_staInterface = address.Assign(m_staDev);

  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxOk", 
                                MakeCallback(&WifiGoodputExperiment::OnTxPacket, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxOk", 
                                MakeCallback(&WifiGoodputExperiment::OnRxPacket, this));
}

void
WifiGoodputExperiment::SetupApplications(bool useTcp, Time simulationTime)
{
  uint16_t port = 9;
  if (useTcp)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(m_staNode.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(simulationTime);

      OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(m_staInterface.GetAddress(0), port));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mb/s")));
      onoff.SetAttribute("PacketSize", UintegerValue(1472));

      ApplicationContainer clientApp = onoff.Install(m_apNode.Get(0));
      clientApp.Start(Seconds(0.5));
      clientApp.Stop(simulationTime);
    }
  else
    {
      UdpServerHelper server(port);
      ApplicationContainer serverApp = server.Install(m_staNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(simulationTime);

      UdpClientHelper client(m_staInterface.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(1472));

      ApplicationContainer clientApp = client.Install(m_apNode.Get(0));
      clientApp.Start(Seconds(0.5));
      clientApp.Stop(simulationTime);
    }
}

void
WifiGoodputExperiment::OnTxPacket(Ptr<const Packet> packet)
{
  ++m_txPackets;
}

void
WifiGoodputExperiment::OnRxPacket(Ptr<const Packet> packet)
{
  ++m_rxPackets;
}

int main(int argc, char* argv[])
{
  uint8_t mcs = 0;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = false;
  double distance = 10.0;
  bool enableRtsCts = false;
  Time simulationTime = Seconds(10);
  bool useTcp = false;
  DataRate expectedThroughput("650Mbps");

  CommandLine cmd(__FILE__);
  cmd.AddValue("mcs", "Modulation and Coding Scheme (0-7)", mcs);
  cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
  cmd.AddValue("distance", "Distance between AP and station (in meters)", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake (true/false)", enableRtsCts);
  cmd.AddValue("simulationTime", "Duration of the simulation (seconds)", simulationTime);
  cmd.AddValue("useTcp", "Use TCP instead of UDP (true/false)", useTcp);
  cmd.AddValue("expectedThroughput", "Expected throughput as data rate (e.g., 650Mbps)", expectedThroughput);
  cmd.Parse(argc, argv);

  WifiGoodputExperiment experiment;
  experiment.Run(mcs, channelWidth, shortGuardInterval, distance, enableRtsCts, 
                 simulationTime, useTcp, expectedThroughput);

  return 0;
}