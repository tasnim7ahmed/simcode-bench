#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultiRateExperiment");

class WifiMultiRateExperiment {
public:
  WifiMultiRateExperiment();
  ~WifiMultiRateExperiment();

  void Run(uint32_t numNodes, uint32_t flows, Time duration, bool enableLogging);
  void SetupApplications(NodeContainer& nodes, uint32_t flows, Time startTime, Time stopTime);
  void PacketRxDrop(Ptr<const Packet> packet);
  void CalculateThroughput();
  void EnableGnuplotAndTracing(std::string plotFileName, std::string dataFileName);

private:
  uint64_t m_bytesReceived;
  Gnuplot2dDataset m_gnuplotDataset;
};

WifiMultiRateExperiment::WifiMultiRateExperiment()
  : m_bytesReceived(0), m_gnuplotDataset("Throughput")
{
}

WifiMultiRateExperiment::~WifiMultiRateExperiment()
{
}

void
WifiMultiRateExperiment::Run(uint32_t numNodes, uint32_t flows, Time duration, bool enableLogging)
{
  if (enableLogging) {
    LogComponentEnable("WifiMultiRateExperiment", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"),
                               "ControlMode", StringValue("OfdmRate6Mbps"));

  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(10),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(nodes);

  SetupApplications(nodes, flows, Seconds(1.0), duration - Seconds(1.0));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                                MakeCallback(&WifiMultiRateExperiment::PacketRxDrop, this));
}

void
WifiMultiRateExperiment::SetupApplications(NodeContainer& nodes, uint32_t flows, Time startTime, Time stopTime)
{
  for (uint32_t i = 0; i < flows; ++i) {
    uint32_t src = i % nodes.GetN();
    uint32_t dst = (i + 1) % nodes.GetN();
    if (src == dst) continue;

    InetSocketAddress sinkAddr = InetSocketAddress(nodes.Get(dst)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddr);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps = onoff.Install(nodes.Get(src));
    apps.Start(startTime);
    apps.Stop(stopTime);

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(dst));
    sinkApps.Start(startTime);
    sinkApps.Stop(stopTime);
  }
}

void
WifiMultiRateExperiment::PacketRxDrop(Ptr<const Packet> packet)
{
  m_bytesReceived += packet->GetSize();
}

void
WifiMultiRateExperiment::CalculateThroughput()
{
  double throughput = static_cast<double>(m_bytesReceived * 8) / (Simulator::Now().GetSeconds() * 1000000); // Mbps
  NS_LOG_INFO("Throughput: " << throughput << " Mbps");
  m_gnuplotDataset.Add(Simulator::Now().GetSeconds(), throughput);
}

void
WifiMultiRateExperiment::EnableGnuplotAndTracing(std::string plotFileName, std::string dataFileName)
{
  Gnuplot gnuplot(plotFileName);
  gnuplot.SetTitle("Throughput vs Time");
  gnuplot.SetXLabel("Time (s)");
  gnuplot.SetYLabel("Throughput (Mbps)");

  Gnuplot2dFileDataset dataset(dataFileName);
  dataset.AddDataset(m_gnuplotDataset);

  gnuplot.AddDataset(dataset);
  std::ofstream plotFile(plotFileName.c_str());
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int main(int argc, char* argv[])
{
  uint32_t numNodes = 100;
  uint32_t flows = 10;
  double durationSec = 10.0;
  bool enableLogging = false;
  std::string plotFileName = "throughput.plot";
  std::string dataFileName = "throughput.data";

  CommandLine cmd(__FILE__);
  cmd.AddValue("numNodes", "Number of WiFi nodes.", numNodes);
  cmd.AddValue("flows", "Number of simultaneous UDP flows.", flows);
  cmd.AddValue("duration", "Simulation duration in seconds.", durationSec);
  cmd.AddValue("enableLogging", "Enable logging output.", enableLogging);
  cmd.Parse(argc, argv);

  Time duration = Seconds(durationSec);

  WifiMultiRateExperiment experiment;
  experiment.Run(numNodes, flows, duration, enableLogging);

  Simulator::Stop(duration);
  Simulator::Run();

  experiment.CalculateThroughput();
  experiment.EnableGnuplotAndTracing(plotFileName, dataFileName);

  Simulator::Destroy();
  return 0;
}