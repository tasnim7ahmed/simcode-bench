#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class WifiStats {
public:
  WifiStats(std::string powerFile, std::string rateFile, std::string throughputFile);
  void PhyTxBegin(Ptr<const Packet> packet);
  void PhyTxDrop(Ptr<const Packet> packet);
  void PhyRxEnd(Ptr<const Packet> packet);
  void UpdateThroughput();

private:
  Gnuplot2dDataset m_powerData;
  Gnuplot2dDataset m_rateData;
  Gnuplot2dDataset m_throughputData;
  uint64_t m_bytesReceived;
  Time m_lastThroughputTime;
};

WifiStats::WifiStats(std::string powerFile, std::string rateFile, std::string throughputFile)
  : m_powerData("Transmit Power"),
    m_rateData("Transmission Rate"),
    m_throughputData("Throughput"),
    m_bytesReceived(0),
    m_lastThroughputTime(Seconds(0))
{
  Gnuplot powerPlot = Gnuplot(powerFile);
  powerPlot.AddDataset(m_powerData);
  powerPlot.SetTitle("Average Transmit Power vs Distance");
  powerPlot.SetXTitle("Distance (m)");
  powerPlot.SetYTitle("Power (dBm)");
  std::ofstream powerStream(powerFile.c_str());
  powerPlot.GenerateOutput(powerStream);
  powerStream.close();

  Gnuplot ratePlot = Gnuplot(rateFile);
  ratePlot.AddDataset(m_rateData);
  ratePlot.SetTitle("Average Transmission Rate vs Distance");
  ratePlot.SetXTitle("Distance (m)");
  ratePlot.SetYTitle("Rate (Mbps)");
  std::ofstream rateStream(rateFile.c_str());
  ratePlot.GenerateOutput(rateStream);
  rateStream.close();

  Gnuplot throughputPlot = Gnuplot(throughputFile);
  throughputPlot.AddDataset(m_throughputData);
  throughputPlot.SetTitle("Throughput vs Distance");
  throughputPlot.SetXTitle("Distance (m)");
  throughputPlot.SetYTitle("Throughput (Mbps)");
  std::ofstream throughputStream(throughputFile.c_str());
  throughputPlot.GenerateOutput(throughputStream);
  throughputStream.close();
}

void WifiStats::PhyTxBegin(Ptr<const Packet> packet) {
  WifiMacHeader hdr;
  packet->PeekHeader(hdr);
  if (hdr.IsData()) {
    double power = 17.0; // Default max for Rrpaa
    auto txVectorTag = DynamicCast<WifiTxVectorTag>(packet->GetTagIterator().Next());
    if (txVectorTag) {
      power = txVectorTag->GetTxVector().GetPowerLevel();
    }
    m_powerData.Add(Simulator::Now().GetSeconds(), power);
  }
}

void WifiStats::PhyTxDrop(Ptr<const Packet> packet) {
  WifiMacHeader hdr;
  packet->PeekHeader(hdr);
  if (hdr.IsData()) {
    m_bytesReceived -= packet->GetSize();
  }
}

void WifiStats::PhyRxEnd(Ptr<const Packet> packet) {
  WifiMacHeader hdr;
  packet->PeekHeader(hdr);
  if (hdr.IsData()) {
    m_bytesReceived += packet->GetSize();
  }
}

void WifiStats::UpdateThroughput() {
  double now = Simulator::Now().GetSeconds();
  double interval = now - m_lastThroughputTime.GetSeconds();
  if (interval > 0) {
    double throughput = (m_bytesReceived * 8.0 / interval) / 1e6;
    m_throughputData.Add(Simulator::Now().GetSeconds(), throughput);
    m_bytesReceived = 0;
    m_lastThroughputTime = Seconds(now);
  }
  Simulator::Schedule(Seconds(1), &WifiStats::UpdateThroughput, this);
}

int main(int argc, char *argv[]) {
  std::string rateManager = "ParfWifiManager";
  std::string powerFile = "power.plt";
  std::string rateFile = "rate.plt";
  std::string throughputFile = "throughput.plt";
  uint32_t rtsThreshold = 2346;

  CommandLine cmd(__FILE__);
  cmd.AddValue("manager", "Rate control manager (ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", rateManager);
  cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
  cmd.AddValue("powerFile", "Gnuplot file for transmit power output", powerFile);
  cmd.AddValue("rateFile", "Gnuplot file for transmission rate output", rateFile);
  cmd.AddValue("throughputFile", "Gnuplot file for throughput output", throughputFile);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2346));

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::" + rateManager);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(1.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNode);
  wifiStaNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(1, 0, 0));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

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
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(1472));
  ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
  clientApp.Start(Seconds(0.0));
  clientApp.Stop(Seconds(20.0));

  WifiStats stats(powerFile, rateFile, throughputFile);
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&WifiStats::PhyTxBegin, &stats));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&WifiStats::PhyTxDrop, &stats));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback(&WifiStats::PhyRxEnd, &stats));

  stats.UpdateThroughput();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}