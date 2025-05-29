#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

class Experiment {
public:
  Experiment(double simulationTime, std::string experimentName)
    : m_simulationTime(simulationTime), m_experimentName(experimentName) {}

  void SetUpNodes() {
    nodes.Create(2);
    staNode = nodes.Get(0);
    apNode = nodes.Get(1);
  }

  void SetUpWifi() {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    phy.Set("ChannelNumber", UintegerValue(1));
    phy.Set("ShortGuardEnabled", BooleanValue(true));

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));

    channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    staDevs = wifi.Install(phy, staNode);
    apDevs = wifi.Install(phy, apNode);
    Ssid ssid = Ssid("wifi-experiment");
    WifiConfiguration conf;
    conf.SetSsid(ssid);
    wifi.Configure(staDevs, conf);
    wifi.Configure(apDevs, conf);
  }

  void SetUpMobility() {
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(10.0),
                                   "GridWidth", UintegerValue(3),
                                   "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    Ptr<ConstantPositionMobilityModel> staMobility =
        staNode->GetObject<ConstantPositionMobilityModel>();
    staMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<ConstantPositionMobilityModel> apMobility =
        apNode->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(10.0, 0.0, 0.0));
  }

  void SetUpInternetStack() {
    internet.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(apDevs);
    address.Assign(staDevs);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  void SetUpApplications(DataRate dataRate) {
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(m_simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(400));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    clientApps = echoClient.Install(staNode);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(m_simulationTime));
  }

  void RunSimulation(std::string dataRate) {
    phy.EnableMonitor(true);

    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
        MakeCallback(&Experiment::RxSnifferTrace, this));

    Simulator::Stop(Seconds(m_simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    WriteResults(dataRate);
  }

  void WriteResults(std::string dataRate) {
    std::ofstream os(m_experimentName + "_" + dataRate + ".dat");
    if (!os.is_open()) {
      std::cerr << "Could not open file " << m_experimentName + "_" + dataRate
                << ".dat for writing. Exiting." << std::endl;
      return;
    }

    for (auto const& [rss, packets] : rssPacketCounts) {
      os << rss << " " << packets << std::endl;
    }

    os.close();

    std::string plotFile = m_experimentName + "_" + dataRate + ".plt";
    std::ofstream plotStream(plotFile.c_str());

    plotStream << "set terminal postscript eps enhanced color font 'Helvetica,14'" << std::endl;
    plotStream << "set output '" << m_experimentName + "_" + dataRate << ".eps'" << std::endl;
    plotStream << "set title 'RSS vs. Packets Received (" << dataRate << ")'" << std::endl;
    plotStream << "set xlabel 'Received Signal Strength (dBm)'" << std::endl;
    plotStream << "set ylabel 'Number of Packets Received'" << std::endl;
    plotStream << "plot '" << m_experimentName + "_" + dataRate
               << ".dat' using 1:2 with linespoints title 'Packets'" << std::endl;
    plotStream.close();
  }

  void RxSnifferTrace(Ptr<const Packet> packet, uint16_t channelFreqMhz,
                      WifiTxVector txVector, MpduInfo mpduInfo, uint8_t preambleType,
                      uint8_t numRxAntennas, double rxNoiseDbm,
                      SignalNoiseDbm signalNoise, uint16_t staId) {
    double rss = signalNoise.signal;
    rssPacketCounts[rss]++;
  }

private:
  double m_simulationTime;
  std::string m_experimentName;

  NodeContainer nodes;
  Ptr<Node> staNode;
  Ptr<Node> apNode;
  WifiHelper wifi;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel;
  NetDeviceContainer staDevs;
  NetDeviceContainer apDevs;
  MobilityHelper mobility;
  InternetStackHelper internet;
  Ipv4InterfaceContainer interfaces;
  ApplicationContainer clientApps;
  std::map<double, int> rssPacketCounts;
};

int main(int argc, char* argv[]) {
  CommandLine cmd;
  double simulationTime = 10.0;
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  std::vector<std::string> dataRates = {"DsssRate1Mbps", "DsssRate2Mbps",
                                         "DsssRate5_5Mbps", "DsssRate11Mbps"};

  for (const auto& rate : dataRates) {
    Experiment experiment(simulationTime, "wifi_rss_experiment");
    experiment.SetUpNodes();
    experiment.SetUpWifi();
    experiment.SetUpMobility();
    experiment.SetUpInternetStack();
    experiment.SetUpApplications(DataRate(rate));
    experiment.RunSimulation(rate);
  }

  return 0;
}