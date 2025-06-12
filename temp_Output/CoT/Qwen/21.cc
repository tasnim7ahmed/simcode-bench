#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRssPerformanceExperiment");

class Experiment : public Object
{
public:
  static TypeId GetTypeId(void);
  Experiment();
  virtual ~Experiment();

  void Setup(uint32_t dataRateIndex, double distance);
  void Run();
  void Teardown();

  uint32_t GetPacketsReceived() const;

private:
  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterface;
  UdpEchoServerHelper m_server;
  UdpEchoClientHelper m_client;
  Config::SetStackInstallCallback m_stackInstaller;
  GnuplotDataset m_rxDataset;

  uint32_t m_packetsReceived;
  Time m_simulationTime;

  void PacketRx(Ptr<const Packet> packet, const Address& from);
  void SetupMobility(double distance);
};

TypeId
Experiment::GetTypeId(void)
{
  static TypeId tid = TypeId("Experiment")
    .SetParent<Object>()
    .AddConstructor<Experiment>();
  return tid;
}

Experiment::Experiment()
  : m_packetsReceived(0),
    m_simulationTime(Seconds(10.0))
{
  m_server = UdpEchoServerHelper(9);
  m_client = UdpEchoClientHelper(Ipv4Address(), 9);
  m_client.SetAttribute("MaxPackets", UintegerValue(1000));
  m_client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  m_client.SetAttribute("PacketSize", UintegerValue(1024));
}

Experiment::~Experiment()
{
}

void
Experiment::Setup(uint32_t dataRateIndex, double distance)
{
  m_apNode.Create(1);
  m_staNode.Create(1);

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  std::vector<std::string> dataRates = {
      "1Mbps", "2Mbps", "5.5Mbps", "11Mbps"
  };
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(dataRates[dataRateIndex]),
                               "ControlMode", StringValue(dataRates[0]));

  wifiMac.SetType("ns3::ApWifiMac");
  m_apDev = wifi.Install(wifiPhy, wifiMac, m_apNode.Get(0));

  wifiMac.SetType("ns3::StaWifiMac");
  m_staDev = wifi.Install(wifiPhy, wifiMac, m_staNode.Get(0));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterface = address.Assign(m_apDev);
  m_staInterface = address.Assign(m_staDev);

  m_server.SetAttribute("Port", UintegerValue(9));
  ApplicationContainer serverApp = m_server.Install(m_staNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(m_simulationTime);

  m_client.SetAttribute("RemoteAddress", AddressValue(m_staInterface.GetAddress(0)));
  ApplicationContainer clientApp = m_client.Install(m_apNode.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(m_simulationTime);

  SetupMobility(distance);

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx", MakeCallback(&Experiment::PacketRx, this));
}

void
Experiment::Run()
{
  Simulator::Stop(m_simulationTime);
  Simulator::Run();
}

void
Experiment::Teardown()
{
  Simulator::Destroy();
  m_apNode.Clear();
  m_staNode.Clear();
}

void
Experiment::PacketRx(Ptr<const Packet> /*packet*/, const Address& /*from*/)
{
  m_packetsReceived++;
}

uint32_t
Experiment::GetPacketsReceived() const
{
  return m_packetsReceived;
}

void
Experiment::SetupMobility(double distance)
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
  positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNode);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("WifiRssPerformanceExperiment", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Gnuplot plot("wifi-rss-performance.eps");
  plot.SetTerminal("postscript eps color");
  plot.SetTitle("RSS vs Packets Received");
  plot.SetXTitle("Distance (m)");
  plot.SetYTitle("Packets Received");

  GnuplotDataset dataset;
  dataset.SetStyle(GnuplotDataset::LINES_POINTS);

  Experiment experiment;

  for (uint32_t rateIndex = 0; rateIndex < 4; ++rateIndex)
  {
    GnuplotDataset localDataset;
    std::stringstream title;
    title << "Rate: " << (rateIndex == 0 ? "1Mbps" : rateIndex == 1 ? "2Mbps" : rateIndex == 2 ? "5.5Mbps" : "11Mbps");
    localDataset.SetTitle(title.str());
    localDataset.SetStyle(GnuplotDataset::LINES_POINTS);

    for (double distance = 1.0; distance <= 100.0; distance += 5.0)
    {
      NS_LOG_INFO("Running simulation with distance: " << distance << "m and data rate index: " << rateIndex);
      experiment.Setup(rateIndex, distance);
      experiment.Run();
      uint32_t packetsReceived = experiment.GetPacketsReceived();
      NS_LOG_INFO("Packets received at distance " << distance << "m: " << packetsReceived);
      localDataset.Add(distance, packetsReceived);
      experiment.Teardown();
    }

    plot.AddDataset(localDataset);
  }

  std::ofstream plotFile("wifi-rss-performance.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}