#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class ThroughputTracker {
public:
  ThroughputTracker(std::string powerFile, std::string rateFile, std::string throughputFile)
    : m_powerPlot(powerFile), m_ratePlot(rateFile), m_throughputPlot(throughputFile) {
    m_powerPlot.SetTitle("Transmit Power vs Distance");
    m_powerPlot.SetXTitle("Distance (m)");
    m_powerPlot.SetYTitle("Tx Power (dBm)");
    m_ratePlot.SetTitle("Data Rate vs Distance");
    m_ratePlot.SetXTitle("Distance (m)");
    m_ratePlot.SetYTitle("Data Rate (Mbps)");
    m_throughputPlot.SetTitle("Throughput vs Distance");
    m_throughputPlot.SetXTitle("Distance (m)");
    m_throughputPlot.SetYTitle("Throughput (Mbps)");
  }

  void LogPhyTxBegin(Ptr<const Packet> packet, WifiTxVector txVector, double txPower, uint16_t channelWidth, uint8_t nss) {
    double distance = m_mobilitySta->GetPosition().x;
    double rate = (txVector.GetMode().GetDataRate(channelWidth, nss, txVector.IsShortGuardInterval()) / 1e6);
    m_ratePlot.AddXY(distance, rate);
    m_powerPlot.AddXY(distance, txPower);
  }

  void SetMobilitySta(Ptr<MobilityModel> mobilitySta) {
    m_mobilitySta = mobilitySta;
  }

  void TrackThroughput(Ptr<const Packet> packet, const Address &) {
    m_totalBytes += packet->GetSize();
  }

  void Report() {
    static uint32_t intervalCount = 0;
    double throughput = (m_totalBytes * 8.0 / 1e6) / 1.0; // Mbps over 1s intervals
    double distance = m_mobilitySta->GetPosition().x;
    m_throughputPlot.AddXY(distance, throughput);
    m_totalBytes = 0;
    intervalCount++;
  }

  void WriteFiles() {
    m_powerPlot.GenerateOutput(&std::cout);
    m_ratePlot.GenerateOutput(&std::cout);
    m_throughputPlot.GenerateOutput(&std::cout);
  }

private:
  Gnuplot2dDataset m_powerPlot;
  Gnuplot2dDataset m_ratePlot;
  Gnuplot2dDataset m_throughputPlot;
  Ptr<MobilityModel> m_mobilitySta;
  double m_totalBytes = 0;
};

int main(int argc, char *argv[]) {
  std::string managerType = "ParfWifiManager";
  uint32_t rtsThreshold = 2346;
  std::string powerFile = "tx-power.plt";
  std::string rateFile = "data-rate.plt";
  std::string throughputFile = "throughput.plt";

  CommandLine cmd(__FILE__);
  cmd.AddValue("manager", "WiFi rate control manager (ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", managerType);
  cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
  cmd.AddValue("powerFile", "Output file for transmit power plot", powerFile);
  cmd.AddValue("rateFile", "Output file for data rate plot", rateFile);
  cmd.AddValue("throughputFile", "Output file for throughput plot", throughputFile);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2200));

  NodeContainer wifiApNode;
  wifiApNode.Create(1);
  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::" + managerType);

  mac.SetType("ns3::StaWifiMac");
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(0.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNode);
  Ptr<MobilityModel> staMobility = wifiStaNode.Get(0)->GetObject<MobilityModel>();
  staMobility->SetPosition(Vector(0, 0, 0));
  DynamicCast<ConstantVelocityMobilityModel>(staMobility)->SetVelocity(Vector(1, 0, 0));

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(20.0));

  UdpClientHelper client(staInterface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  client.SetAttribute("PacketSize", UintegerValue(1472));
  ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
  clientApp.Start(Seconds(0.0));
  clientApp.Stop(Seconds(20.0));

  ThroughputTracker tracker(powerFile, rateFile, throughputFile);
  tracker.SetMobilitySta(staMobility);
  Config::Connect("/NodeList/1/ApplicationList/0/Rx", MakeCallback(&ThroughputTracker::TrackThroughput, &tracker));

  Config::ConnectWithoutContext("/NodeList/1/DeviceList/0/Phy/TxBegin", MakeCallback(&ThroughputTracker::LogPhyTxBegin, &tracker));

  Simulator::ScheduleRepeatingEvent(Seconds(1), MakeEvent(&ThroughputTracker::Report, &tracker));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  tracker.WriteFiles();
  Simulator::Destroy();

  return 0;
}