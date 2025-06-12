#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment();
  void Configure(int argc, char **argv);
  void Run();
  void Report();

private:
  void CreateNodes();
  void CreateDevices();
  void InstallInternetStack();
  void InstallApplications();
  void SetMobility();
  void SetupFlowMonitor();
  void ScheduleThroughputCalculation();
  void CalculateThroughput();
  void ReceivePacket(Ptr<Socket> socket);

  uint32_t m_nNodes;
  double m_simulationTime;
  double m_areaSize;
  uint32_t m_nFlows;
  bool m_verbose;
  std::string m_phyMode;
  std::string m_wifiRate;
  std::string m_outputFile;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  std::vector<Ptr<Socket>> m_receiveSockets;
  std::vector<uint64_t> m_totalBytesReceived;
  Gnuplot m_gnuplot;
  FlowMonitorHelper m_flowHelper;
  Ptr<FlowMonitor> m_monitor;
};

WifiMultirateExperiment::WifiMultirateExperiment()
    : m_nNodes(100),
      m_simulationTime(60.0),
      m_areaSize(500.0),
      m_nFlows(20),
      m_verbose(false),
      m_phyMode("HtMcs7"),
      m_wifiRate("54Mbps"),
      m_outputFile("wifi-multirate-experiment-throughput.png"),
      m_gnuplot("Throughput over Time", "Time (s)", "Aggregate Throughput (Mbps)")
{
}

void WifiMultirateExperiment::Configure(int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue("nNodes", "Number of WiFi nodes", m_nNodes);
  cmd.AddValue("simulationTime", "Simulation time in seconds", m_simulationTime);
  cmd.AddValue("areaSize", "Area size for mobility model (meters)", m_areaSize);
  cmd.AddValue("nFlows", "Number of simultaneous flows", m_nFlows);
  cmd.AddValue("verbose", "Enable verbose logging", m_verbose);
  cmd.AddValue("phyMode", "WiFi PHY mode", m_phyMode);
  cmd.AddValue("wifiRate", "Offered rate by OnOffApplication", m_wifiRate);
  cmd.Parse(argc, argv);

  if (m_verbose)
  {
    LogComponentEnable("WifiMultirateExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  }
}

void WifiMultirateExperiment::CreateNodes()
{
  m_nodes.Create(m_nNodes);
}

void WifiMultirateExperiment::SetMobility()
{
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
      "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
      "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
      "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=" + std::to_string(m_areaSize) + "|MaxY=" + std::to_string(m_areaSize) + "]"));
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
      "MinX", DoubleValue(0.0),
      "MinY", DoubleValue(0.0),
      "DeltaX", DoubleValue(10.0),
      "DeltaY", DoubleValue(10.0),
      "GridWidth", UintegerValue(10),
      "LayoutType", StringValue("RowFirst"));
  mobility.Install(m_nodes);
}

void WifiMultirateExperiment::CreateDevices()
{
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager", "RtsCtsThreshold", StringValue("2200"));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());
  phy.SetErrorRateModel("ns3::YansErrorRateModel");

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  m_devices = wifi.Install(phy, mac, m_nodes);
}

void WifiMultirateExperiment::InstallInternetStack()
{
  OlsrHelper olsr;
  InternetStackHelper stack;
  stack.SetRoutingHelper(olsr);
  stack.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.0.0");
  m_interfaces = address.Assign(m_devices);
}

void WifiMultirateExperiment::InstallApplications()
{
  uint16_t port = 5000;
  ApplicationContainer apps;

  Ptr<UniformRandomVariable> randomNode = CreateObject<UniformRandomVariable>();
  m_receiveSockets.resize(m_nNodes);
  m_totalBytesReceived.clear();
  m_totalBytesReceived.resize(m_nNodes, 0);

  for (uint32_t i = 0; i < m_nFlows; ++i)
  {
    uint32_t src, dst;
    do
    {
      src = randomNode->GetValue(0, m_nNodes);
      dst = randomNode->GetValue(0, m_nNodes);
    } while (src == dst);

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(m_interfaces.GetAddress(dst), port));
    onoff.SetAttribute("DataRate", StringValue(m_wifiRate));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + double(i) * 0.1)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(m_simulationTime - 1)));
    apps.Add(onoff.Install(m_nodes.Get(src)));

    if (m_receiveSockets[dst] == nullptr)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      Ptr<Socket> recvSink = Socket::CreateSocket(m_nodes.Get(dst), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
      recvSink->Bind(local);
      recvSink->SetRecvCallback(MakeCallback(&WifiMultirateExperiment::ReceivePacket, this));
      m_receiveSockets[dst] = recvSink;
    }
  }
}

void WifiMultirateExperiment::SetupFlowMonitor()
{
  m_monitor = m_flowHelper.InstallAll();
}

void WifiMultirateExperiment::ReceivePacket(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
  {
    for (uint32_t i = 0; i < m_receiveSockets.size(); ++i)
    {
      if (m_receiveSockets[i] == socket)
      {
        m_totalBytesReceived[i] += packet->GetSize();
        break;
      }
    }
  }
}

void WifiMultirateExperiment::ScheduleThroughputCalculation()
{
  Simulator::Schedule(Seconds(1.0), &WifiMultirateExperiment::CalculateThroughput, this);
}

void WifiMultirateExperiment::CalculateThroughput()
{
  double timeNow = Simulator::Now().GetSeconds();
  uint64_t totalBytes = 0;
  for (uint32_t i = 0; i < m_totalBytesReceived.size(); ++i)
  {
    totalBytes += m_totalBytesReceived[i];
    m_totalBytesReceived[i] = 0;
  }
  double throughputMbps = (totalBytes * 8.0) / 1e6;
  m_gnuplot.AddDataset(Gnuplot2dDataset(timeNow, throughputMbps));
  if (timeNow < m_simulationTime - 1.0)
  {
    Simulator::Schedule(Seconds(1.0), &WifiMultirateExperiment::CalculateThroughput, this);
  }
}

void WifiMultirateExperiment::Run()
{
  CreateNodes();
  SetMobility();
  CreateDevices();
  InstallInternetStack();
  InstallApplications();
  SetupFlowMonitor();

  ScheduleThroughputCalculation();

  Simulator::Stop(Seconds(m_simulationTime));
  Simulator::Run();

  m_monitor->SerializeToXmlFile("flow-monitor.xml", true, true);

  Simulator::Destroy();
}

void WifiMultirateExperiment::Report()
{
  std::ofstream plotFile(m_outputFile);
  m_gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int main(int argc, char *argv[])
{
  WifiMultirateExperiment experiment;
  experiment.Configure(argc, argv);
  experiment.Run();
  experiment.Report();
  std::cout << "Simulation complete. Results written to " << experiment.m_outputFile << std::endl;
  std::cout << "Gnuplot ready. Build: ./waf; Run: ./waf --run \"<program> [args]\"" << std::endl;
  std::cout << "For logging/debugging, use --verbose=1" << std::endl;
  return 0;
}