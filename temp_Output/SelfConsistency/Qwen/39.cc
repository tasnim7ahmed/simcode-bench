#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifi");

class RateAdaptiveWifi : public Object
{
public:
  static TypeId GetTypeId(void);
  RateAdaptiveWifi();
  virtual ~RateAdaptiveWifi();

  void SetSimulationTime(double t) { m_simTime = t; }
  void SetDistanceStep(double s) { m_distanceStep = s; }
  void SetDistanceMax(double d) { m_maxDistance = d; }
  void SetEnableLogging(bool l) { m_enableLogging = l; }

private:
  virtual void DoRun(void);

  double m_simTime;
  double m_distanceStep;
  double m_maxDistance;
  bool m_enableLogging;

  std::vector<double> m_distances;
  std::vector<double> m_throughputs;
};

TypeId
RateAdaptiveWifi::GetTypeId(void)
{
  static TypeId tid = TypeId("RateAdaptiveWifi")
    .SetParent<Object>()
    .AddConstructor<RateAdaptiveWifi>();
  return tid;
}

RateAdaptiveWifi::RateAdaptiveWifi()
  : m_simTime(10.0),
    m_distanceStep(5.0),
    m_maxDistance(100.0),
    m_enableLogging(false)
{
}

RateAdaptiveWifi::~RateAdaptiveWifi()
{
}

void
RateAdaptiveWifi::DoRun(void)
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("simTime", "Total simulation time (seconds)", m_simTime);
  cmd.AddValue("distanceStep", "Distance step between STA and AP (meters)", m_distanceStep);
  cmd.AddValue("maxDistance", "Maximum distance to simulate (meters)", m_maxDistance);
  cmd.AddValue("enableLogging", "Enable logging output", m_enableLogging);
  cmd.Parse(Simulator::Get argc, Simulator::Get argv);

  if (m_enableLogging)
  {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("RateAdaptiveWifi", LOG_LEVEL_INFO);
  }

  NodeContainer apNode;
  NodeContainer staNode;
  apNode.Create(1);
  staNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager"); // Minstrel for rate adaptation

  Ssid ssid = Ssid("rate-adaptive-wifi");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(staNode);

  Vector initialPosition(0.0, 0.0, 0.0);
  Vector velocity(1.0, 0.0, 0.0); // Move along x-axis
  staNode.Get(0)->GetObject<MobilityModel>()->SetPosition(initialPosition);
  staNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(velocity);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(staNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(m_simTime));

  UdpClientHelper client(staInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295)); // Unlimited packets
  client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));   // High rate
  client.SetAttribute("PacketSize", UintegerValue(1472));        // UDP payload size

  ApplicationContainer clientApp = client.Install(apNode.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(m_simTime));

  Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(
      [](Ptr<const Packet> p, const Address &) {
        static uint64_t totalRxBytes = 0;
        totalRxBytes += p->GetSize();
        static Time lastLog = Seconds(0);
        if (Simulator::Now() - lastLog >= Seconds(1))
        {
          double throughput = (totalRxBytes * 8) / (Simulator::Now().GetSeconds() - lastLog.GetSeconds()) / 1e6;
          NS_LOG_INFO("Throughput: " << throughput << " Mbps");
          lastLog = Simulator::Now();
        }
      }));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi-rate-adaptive.tr");
  phy.EnableAsciiAll(stream);

  double currentTime = 0.0;
  double nextDistanceChange = m_distanceStep / velocity.x;

  while (currentTime <= m_maxDistance / velocity.x && currentTime <= m_simTime)
  {
    Simulator::Stop(Seconds(nextDistanceChange));
    Simulator::Run();

    Vector pos = staNode.Get(0)->GetObject<MobilityModel>()->GetPosition();
    double distance = pos.x;
    m_distances.push_back(distance);

    double rxBytes = 0;
    auto sink = DynamicCast<UdpServer>(serverApp.Get(0));
    rxBytes = sink->GetReceived();
    double throughput = (rxBytes * 8) / (Simulator::Now().GetSeconds()) / 1e6;
    m_throughputs.push_back(throughput);

    NS_LOG_UNCOND("Distance: " << distance << " m | Throughput: " << throughput << " Mbps");

    nextDistanceChange += m_distanceStep / velocity.x;
    currentTime = Simulator::Now().GetSeconds();
  }

  Simulator::Destroy();

  Gnuplot gnuplot("throughput-vs-distance.png");
  gnuplot.SetTitle("Throughput vs Distance");
  gnuplot.SetXTitle("Distance (m)");
  gnuplot.SetYTitle("Throughput (Mbps)");

  Gnuplot2dDataset dataset;
  dataset.SetTitle("Measured Throughput");
  dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  for (size_t i = 0; i < m_distances.size(); ++i)
  {
    dataset.Add(m_distances[i], m_throughputs[i]);
  }

  gnuplot.AddDataset(dataset);

  std::ofstream plotFile("throughput-vs-distance.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char *argv[])
{
  RateAdaptiveWifi experiment;
  experiment.DoRun();
  return 0;
}