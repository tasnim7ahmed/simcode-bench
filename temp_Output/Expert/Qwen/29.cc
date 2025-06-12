#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultiRateExperiment");

class WiFiMultiRateExperiment {
public:
  WiFiMultiRateExperiment();
  void Run(uint32_t numNodes, uint32_t numFlows, std::string phyMode, double distance);
  void SetupApplications();
  void SetupMobility();
  void MonitorPackets(Ptr<OutputStreamWrapper> stream);
  void CalculateThroughput();

private:
  NodeContainer wifiStaNodes;
  NetDeviceContainer staDevices;
  Ipv4InterfaceContainer staInterfaces;
  std::vector<Ptr<Socket>> sinkSockets;
  Time lastTotalRx;
  uint64_t totalRx;
  Gnuplot2dDataset throughputDataset;
};

WiFiMultiRateExperiment::WiFiMultiRateExperiment()
  : totalRx(0), throughputDataset("Throughput")
{
  throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void
WiFiMultiRateExperiment::Run(uint32_t numNodes, uint32_t numFlows, std::string phyMode, double distance)
{
  NS_LOG_INFO("Creating nodes.");
  wifiStaNodes.Create(numNodes);

  NS_LOG_INFO("Setting up mobility.");
  SetupMobility();

  NS_LOG_INFO("Setting up WiFi.");
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  staDevices = wifi.Install(wifiPhy, wifiMac, wifiStaNodes);

  NS_LOG_INFO("Installing internet stack.");
  InternetStackHelper stack;
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  staInterfaces = address.Assign(staDevices);

  NS_LOG_INFO("Setting up applications.");
  SetupApplications();

  NS_LOG_INFO("Enabling logging.");
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("wifi-multi-rate.tr");
  wifiPhy.EnableAsciiAll(stream);

  NS_LOG_INFO("Monitoring packets.");
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeBoundCallback(&WiFiMultiRateExperiment::MonitorPackets, this));

  NS_LOG_INFO("Scheduling throughput calculation.");
  Simulator::Schedule(Seconds(0.5), &WiFiMultiRateExperiment::CalculateThroughput);

  NS_LOG_INFO("Running simulation.");
  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();
}

void
WiFiMultiRateExperiment::SetupApplications()
{
  uint32_t numFlowsPerPair = 1;
  uint16_t port = 9;

  for (uint32_t i = 0; i < numFlows; ++i)
    {
      uint32_t src = rand() % wifiStaNodes.GetN();
      uint32_t dst = rand() % wifiStaNodes.GetN();
      if (src == dst)
        {
          continue;
        }

      for (uint32_t j = 0; j < numFlowsPerPair; ++j)
        {
          InetSocketAddress sinkAddress(staInterfaces.GetAddress(dst), port);
          OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
          onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
          onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
          onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
          onoff.SetAttribute("PacketSize", UintegerValue(1024));

          ApplicationContainer apps = onoff.Install(wifiStaNodes.Get(src));
          apps.Start(Seconds(1.0));
          apps.Stop(Seconds(9.0));

          PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
          ApplicationContainer sinkApps = sink.Install(wifiStaNodes.Get(dst));
          sinkApps.Start(Seconds(0.0));
          sinkApps.Stop(Seconds(10.0));

          port++;
        }
    }
}

void
WiFiMultiRateExperiment::SetupMobility()
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(10),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobility.Install(wifiStaNodes);
}

void
WiFiMultiRateExperiment::MonitorPackets(Ptr<OutputStreamWrapper> stream)
{
  totalRx += 1;
}

void
WiFiMultiRateExperiment::CalculateThroughput()
{
  double kbpsThroughput = (totalRx * 8.0) / 1000;
  NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds() << "s, Throughput: " << kbpsThroughput << " Kbps");
  throughputDataset.Add(Simulator::Now().GetSeconds(), kbpsThroughput);
  totalRx = 0;

  Gnuplot gnuplot("throughput.png");
  gnuplot.SetTitle("Throughput over Time");
  gnuplot.SetTerminal("png");
  gnuplot.AddDataset(throughputDataset);
  std::ofstream plotFile("throughput.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  Simulator::Schedule(Seconds(0.5), &WiFiMultiRateExperiment::CalculateThroughput);
}

int
main(int argc, char* argv[])
{
  uint32_t numNodes = 100;
  uint32_t numFlows = 50;
  std::string phyMode = "OfdmRate6Mbps";
  CommandLine cmd(__FILE__);
  cmd.AddValue("numNodes", "Number of nodes in the network", numNodes);
  cmd.AddValue("numFlows", "Number of simultaneous flows", numFlows);
  cmd.AddValue("phyMode", "WiFi Physical layer mode", phyMode);
  cmd.Parse(argc, argv);

  WiFiMultiRateExperiment experiment;
  experiment.Run(numNodes, numFlows, phyMode, 50.0);

  return 0;
}