#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class WifiLogger {
public:
  WifiLogger(std::string throughputFile, std::string powerFile)
    : m_throughputFile(throughputFile), m_powerFile(powerFile) {}

  void LogRateAndPower(uint32_t nodeId, uint8_t txPower, DataRate rate) {
    NS_LOG_INFO("Node " << nodeId << " Tx Power: " << (uint32_t)txPower << " dBm, Rate: " << rate.GetBitrate() / 1e6 << " Mbps");
    m_powerStream << Simulator::Now().GetSeconds() << " " << (uint32_t)txPower << std::endl;
    m_rateStream << Simulator::Now().GetSeconds() << " " << rate.GetBitrate() / 1e6 << std::endl;
  }

  void LogThroughput(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> pkt, const Address&) {
    double now = Simulator::Now().GetSeconds();
    double bits = pkt->GetSize() * 8.0;
    m_totalBits += bits;
    m_lastLogTime = m_startTime;
    m_startTime = now;
    double interval = m_startTime - m_lastLogTime;
    if (interval > 0) {
      double bps = m_totalBits / interval;
      m_throughputStream << now << " " << bps / 1e6 << std::endl; // Mbps
      NS_LOG_INFO("Throughput at " << now << "s: " << bps / 1e6 << " Mbps");
      m_totalBits = 0;
    }
  }

  void Start() {
    m_powerStream.open(m_powerFile);
    m_throughputStream.open(m_throughputFile);
    m_rateStream.open("rate-vs-time.dat");
  }

  void Stop() {
    m_powerStream.close();
    m_throughputStream.close();
    m_rateStream.close();
  }

private:
  std::string m_throughputFile;
  std::string m_powerFile;
  std::ofstream m_powerStream;
  std::ofstream m_throughputStream;
  std::ofstream m_rateStream;

  double m_totalBits = 0.0;
  double m_lastLogTime = 0.0;
  double m_startTime = 0.0;
};

static void MoveSta(Ptr<Node> sta, Vector direction, double speed, double duration) {
  Ptr<MobilityModel> mobility = sta->GetObject<MobilityModel>();
  Vector position = mobility->GetPosition();
  Vector newPos = position + direction * speed * duration;
  mobility->SetPosition(newPos);
  NS_LOG_UNCOND("STA moved to position (" << newPos.x << ", " << newPos.y << ")");
}

int main(int argc, char *argv[]) {
  std::string rateManagerType = "ParfWifiManager";
  uint32_t rtsThreshold = 2346;
  std::string throughputFile = "throughput.dat";
  std::string powerFile = "power.dat";

  CommandLine cmd(__FILE__);
  cmd.AddValue("rateManager", "Rate control manager (ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", rateManagerType);
  cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
  cmd.AddValue("throughputFile", "Output file for throughput data", throughputFile);
  cmd.AddValue("powerFile", "Output file for transmit power data", powerFile);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate54Mbps"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2200));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::" + rateManagerType);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-comparison");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice;
  NodeContainer staNode;
  staNode.Create(1);
  staDevice = wifi.Install(phy, mac, staNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  NodeContainer apNode;
  apNode.Create(1);
  apDevice = wifi.Install(phy, mac, apNode);

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

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(staNode);
  staNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0.0, 1.0, 0.0));

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  Ipv4InterfaceContainer staInterface;
  apInterface = address.Assign(apDevice);
  staInterface = address.Assign(staDevice);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(staNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(20.0));

  UdpClientHelper client(staInterface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(1000000 / 54000000 * 8)));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer clientApp = client.Install(apNode.Get(0));
  clientApp.Start(Seconds(0.0));
  clientApp.Stop(Seconds(20.0));

  WifiLogger logger(throughputFile, powerFile);
  logger.Start();

  Config::Connect("/NodeList/*/DeviceList/*/TxTrace", MakeCallback(&WifiLogger::LogRateAndPower, &logger));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("throughput-trace.dat");
  serverApp.Get(0)->TraceConnectWithoutContext("Rx", MakeBoundCallback(&WifiLogger::LogThroughput, &logger));

  AnimationInterface anim("wifi-rate-control.xml");

  for (double t = 1.0; t <= 20.0; t += 1.0) {
    Simulator::Schedule(Seconds(t), &MoveSta, staNode.Get(0), Vector(0, 1, 0), 1.0, 1.0);
  }

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  logger.Stop();

  Gnuplot throughputPlot("throughput.png");
  throughputPlot.SetTitle("Throughput vs Time");
  throughputPlot.SetXTitle("Time (s)");
  throughputPlot.SetYTitle("Throughput (Mbps)");
  GnuplotDataset throughputData("throughput.dat");
  throughputPlot.AddDataset(throughputData);
  std::ofstream tpOut("throughput.plt");
  tpOut << throughputPlot.GenerateOutput();
  tpOut.close();

  Gnuplot powerPlot("power.png");
  powerPlot.SetTitle("Transmit Power vs Time");
  powerPlot.SetXTitle("Time (s)");
  powerPlot.SetYTitle("Power (dBm)");
  GnuplotDataset powerData("power.dat");
  powerPlot.AddDataset(powerData);
  std::ofstream pwrOut("power.plt");
  pwrOut << powerPlot.GenerateOutput();
  pwrOut.close();

  return 0;
}