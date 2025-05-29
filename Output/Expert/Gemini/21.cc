#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiClearChannelExperiment");

class Experiment {
public:
  Experiment(double distance, std::string rate, int numPackets)
    : m_distance(distance),
      m_rate(rate),
      m_numPackets(numPackets)
  {}

  void Run() {
    CreateNodes();
    InstallWifi();
    InstallInternetStack();
    PositionNodes();
    InstallApplications();
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    AnalyzeResults();
    Simulator::Destroy();
  }

private:
  void CreateNodes() {
    m_nodes.Create(2);
  }

  void InstallWifi() {
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("wifi-experiment");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, m_nodes.Get(1));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, m_nodes.Get(0));

    m_devices.Add(staDevice);
    m_devices.Add(apDevice);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(22));
  }

  void InstallInternetStack() {
    InternetStackHelper stack;
    stack.Install(m_nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    m_interfaces = address.Assign(m_devices);
  }

  void PositionNodes() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(m_distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_nodes);
  }

  void InstallApplications() {
    UdpClientHelper client(m_interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(m_numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(m_nodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(m_nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    Config::Set("/NodeList/*/ApplicationList/*/$ns3::UdpClient/DataRate", StringValue(m_rate));
  }

  void AnalyzeResults() {
    double rssMean = 0;
    uint32_t totalPacketsReceived = 0;

    for (uint32_t i = 0; i < m_devices.GetN(); ++i) {
      Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(m_devices.Get(i));
      if (wifiNetDevice) {
        Ptr<WifiPhy> phy = wifiNetDevice->GetPhy();
        if (phy) {
          rssMean = phy->GetAverageRss();
        }
      }
    }

    Ptr<UdpServer> server = DynamicCast<UdpServer>(m_nodes.Get(0)->GetApplication(0));
    if(server) {
      totalPacketsReceived = server->GetReceived();
    }
    m_rssValues.push_back(rssMean);
    m_packetsReceived.push_back(totalPacketsReceived);

  }

  double GetLastRss() const {
    if (!m_rssValues.empty()) {
      return m_rssValues.back();
    }
    return 0.0;
  }

  uint32_t GetLastPacketsReceived() const {
      if (!m_packetsReceived.empty()) {
          return m_packetsReceived.back();
      }
      return 0;
  }

  std::vector<double> GetRssValues() const { return m_rssValues; }
  std::vector<uint32_t> GetPacketsReceived() const { return m_packetsReceived; }

private:
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  double m_distance;
  std::string m_rate;
  int m_numPackets;
  std::vector<double> m_rssValues;
  std::vector<uint32_t> m_packetsReceived;
};

int main(int argc, char *argv[]) {
  LogComponentEnable("WifiClearChannelExperiment", LOG_LEVEL_INFO);

  double distance = 10.0;
  int numPackets = 1000;
  std::vector<std::string> rates = {"1Mbps", "2Mbps", "5.5Mbps", "11Mbps"};

  std::vector<double> distances = {5.0, 10.0, 20.0, 30.0};

  Gnuplot2dDataset dataset;
  dataset.SetTitle ("Packets Received vs. RSS");
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for (const auto& rate : rates) {
        for(const auto& dist : distances) {
            Experiment experiment(dist, rate, numPackets);
            experiment.Run();

            double lastRss = experiment.GetLastRss();
            uint32_t lastPacketsReceived = experiment.GetLastPacketsReceived();
            std::cout << "Distance: " << dist << " Rate: " << rate << " RSS: " << lastRss << " Packets Received: " << lastPacketsReceived << std::endl;

            dataset.Add (lastRss, lastPacketsReceived);
        }

  }

  Gnuplot plot ("wifi_clear_channel.eps");
  plot.SetTitle ("WiFi Clear Channel Performance");
  plot.SetLegend ("RSS", "Packets Received");
  plot.AddDataset (dataset);
  plot.GenerateOutput ();

  return 0;
}