#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <string>
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class MetricsLogger
{
public:
  MetricsLogger(const std::string& throughputFilename, const std::string& txpowerFilename)
    : m_throughputFilename(throughputFilename),
      m_txpowerFilename(txpowerFilename)
  {
    m_receivedBytes = 0;
    m_lastThroughputTime = Seconds(1.0);
    m_totalTxPower = 0.0;
    m_txPowerSamples = 0;
  }

  void LogRx(Ptr<const Packet> packet, const Address &from)
  {
    m_receivedBytes += packet->GetSize();
  }

  void LogTxPower(double txPower)
  {
    m_totalTxPower += txPower;
    ++m_txPowerSamples;
  }

  void ThroughputStep(double distance, double interval, bool final = false)
  {
    double throughput = (m_receivedBytes * 8.0) / (interval * 1e6); // Mbps
    m_throughput.push_back(std::make_pair(distance, throughput));
    m_receivedBytes = 0;
    if (final)
      WriteThroughput();
  }

  void TxPowerStep(double distance, bool final = false)
  {
    double avgTxPower = m_txPowerSamples > 0 ? m_totalTxPower / m_txPowerSamples : 0.0;
    m_txpower.push_back(std::make_pair(distance, avgTxPower));
    m_totalTxPower = 0.0;
    m_txPowerSamples = 0;
    if (final)
      WriteTxPower();
  }

  void WriteThroughput()
  {
    std::ofstream out(m_throughputFilename, std::ios_base::trunc);
    out << "#Distance(m)\tThroughput(Mbps)" << std::endl;
    for (auto &entry : m_throughput)
      out << entry.first << "\t" << entry.second << std::endl;
    out.close();
  }

  void WriteTxPower()
  {
    std::ofstream out(m_txpowerFilename, std::ios_base::trunc);
    out << "#Distance(m)\tAvgTxPower(dBm)" << std::endl;
    for (auto &entry : m_txpower)
      out << entry.first << "\t" << entry.second << std::endl;
    out.close();
  }

private:
  std::vector<std::pair<double,double>> m_throughput;
  std::vector<std::pair<double,double>> m_txpower;
  uint64_t m_receivedBytes;
  Time m_lastThroughputTime;
  double m_totalTxPower;
  uint32_t m_txPowerSamples;
  std::string m_throughputFilename;
  std::string m_txpowerFilename;
};

struct UserParams
{
  std::string rcManager;
  uint32_t rtsThreshold;
  std::string throughputFilename;
  std::string txpowerFilename;
};

std::string GetManagerName(const std::string& str)
{
  std::map<std::string,std::string> cmap = {
    {"parf", "ns3::ParfWifiManager"},
    {"aparf", "ns3::AparfWifiManager"},
    {"rrpaa", "ns3::RrpaaWifiManager"}
  };
  auto it = cmap.find(str);
  if (it != cmap.end())
    return it->second;
  return str;
}

double GetCurrentTxPower(Ptr<WifiNetDevice> dev)
{
  Ptr<WifiPhy> phy = dev->GetPhy();
  if (phy)
    return phy->GetTxPowerEnd();
  return 0;
}

void LogStationTxParams(Ptr<WifiNetDevice> staDev)
{
  Ptr<WifiMac> mac = staDev->GetMac();
  Ptr<WifiRemoteStationManager> manager = staDev->GetRemoteStationManager();
  Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac>(mac);

  std::cout << Simulator::Now().GetSeconds() << "s: ";
  if (manager)
  {
    uint32_t mode = manager->GetDataMode(0).GetUid();
    std::string rate = manager->GetDataMode(0).GetUniqueName();
    std::cout << "DataMode: " << rate << "; ";
  }
  Ptr<WifiPhy> phy = staDev->GetPhy();
  if (phy)
  {
    std::cout << "TxPower: " << phy->GetTxPowerEnd() << " dBm";
  }
  std::cout << std::endl;
}

void SampleThroughputAndPower(MetricsLogger* logger, double staDistance, double interval, bool last)
{
  logger->ThroughputStep(staDistance, interval, last);
  logger->TxPowerStep(staDistance, last);
}

void RecordTxPower(MetricsLogger* logger, Ptr<WifiNetDevice> dev)
{
  double txPower = GetCurrentTxPower(dev);
  logger->LogTxPower(txPower);
}

void LogTxAndRate(Ptr<WifiNetDevice> dev)
{
  LogStationTxParams(dev);
}

int main(int argc, char *argv[])
{
  UserParams p;
  p.rcManager = "parf";
  p.rtsThreshold = 2347;
  p.throughputFilename = "throughput.dat";
  p.txpowerFilename = "txpower.dat";

  CommandLine cmd;
  cmd.AddValue("rcManager", "WiFi rate control manager: parf|aparf|rrpaa or NS-3 class name", p.rcManager);
  cmd.AddValue("rtsThreshold", "RTS/CTS threshold (bytes)", p.rtsThreshold);
  cmd.AddValue("throughputFilename", "Output filename for throughput vs. distance", p.throughputFilename);
  cmd.AddValue("txpowerFilename", "Output filename for TxPower vs. distance", p.txpowerFilename);
  cmd.Parse(argc, argv);

  std::string rcManagerName = GetManagerName(p.rcManager);

  // Nodes
  NodeContainer wifiNodes;
  wifiNodes.Create(2);
  Ptr<Node> apNode = wifiNodes.Get(0);
  Ptr<Node> staNode = wifiNodes.Get(1);

  // WiFi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(20.0));
  phy.Set("TxPowerEnd", DoubleValue(20.0));
  phy.Set("TxPowerLevels", UintegerValue(1));
  phy.Set("RxGain", DoubleValue(0));
  phy.Set("TxGain", DoubleValue(0));
  phy.Set("EnergyDetectionThreshold", DoubleValue(-99));
  phy.Set("CcaMode1Threshold", DoubleValue(-62));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  wifi.SetRemoteStationManager(rcManagerName,
    "RtsCtsThreshold", UintegerValue(p.rtsThreshold));

  WifiMacHelper mac;

  Ssid ssid = Ssid("sim-ssid");
  // AP
  mac.SetType("ns3::ApWifiMac",
    "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  // STA
  mac.SetType("ns3::StaWifiMac",
    "Ssid", SsidValue(ssid),
    "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
      "MinX", DoubleValue(0.0),
      "MinY", DoubleValue(0.0),
      "DeltaX", DoubleValue(0.0),
      "DeltaY", DoubleValue(0.0),
      "GridWidth", UintegerValue(1),
      "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevices, staDevices));

  // UDP traffic: AP -> STA
  uint16_t port = 4000;
  ApplicationContainer serverApp;
  UdpServerHelper udpServer(port);
  serverApp = udpServer.Install(staNode);
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(50.0));

  uint64_t cbrRate = 54 * 1000000; // 54 Mbps
  UdpClientHelper udpClient(interfaces.GetAddress(1), port);
  udpClient.SetAttribute("MaxPackets", UintegerValue(0));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0 * 1024.0 * 8 / cbrRate))); // pkt every X seconds
  udpClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1024B = 8192 bits

  ApplicationContainer clientApp = udpClient.Install(apNode);
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(50.0));

  // Metrics
  MetricsLogger logger(p.throughputFilename, p.txpowerFilename);

  Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
  server->TraceConnectWithoutContext("Rx", MakeCallback(
    [&logger] (Ptr<const Packet> p, const Address &addr) {
      logger.LogRx(p, addr);
    }
  ));

  // Schedule mobility and metric sampling
  Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
  Ptr<MobilityModel> staMobility = staNode->GetObject<MobilityModel>();
  Ptr<WifiNetDevice> staDev = DynamicCast<WifiNetDevice>(staNode->GetDevice(0));

  double initialDistance = 1.0;
  double maxDistance = 50.0;
  double timeStep = 1.0;
  double simTime = 50.0;
  Vector apPos = Vector(0.0, 0.0, 0.0);

  apMobility->SetPosition(apPos);
  staMobility->SetPosition(Vector(initialDistance, 0.0, 0.0));
  double currentDistance = initialDistance;

  // Schedule events
  for (uint32_t t = 1; currentDistance <= maxDistance && t < simTime; ++t)
  {
    Simulator::Schedule(Seconds(t), [staMobility,currentDistance]() {
      staMobility->SetPosition(Vector(currentDistance, 0.0, 0.0));
    });
    Simulator::Schedule(Seconds(t), [&, currentDistance, timeStep, staDev]() {
      SampleThroughputAndPower(&logger, currentDistance, timeStep, false);
      RecordTxPower(&logger, staDev);
      LogTxAndRate(staDev);
    });
    currentDistance += 1.0;
  }

  // last sample
  Simulator::Schedule(Seconds(simTime), [&](){
    SampleThroughputAndPower(&logger, currentDistance - 1.0, simTime - (currentDistance - initialDistance), true);
    logger.WriteThroughput();
    logger.WriteTxPower();
  });

  Simulator::Stop(Seconds(simTime+1));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}