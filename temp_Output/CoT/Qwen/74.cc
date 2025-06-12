#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

class ManetRoutingComparison
{
public:
  static void
  RxTracer(Ptr<const Packet> packet, const Address &from)
  {
    ++g_totalRx;
  }

  static uint32_t g_totalRx;
};

uint32_t ManetRoutingComparison::g_totalRx = 0;

int
main(int argc, char *argv[])
{
  std::string phyMode("DsssRate11Mbps");
  double distance = 500; // m
  uint32_t numNodes = 50;
  double totalTime = 200.0;
  double dataStart = 50.0;
  double dataEnd = 51.0;
  double txPower = 7.5; // dBm
  double nodeSpeed = 20.0; // m/s
  double areaWidth = 300.0;
  double areaHeight = 1500.0;
  bool enableFlowMonitor = false;
  bool enableMobilityTrace = false;
  std::string csvFileName = "manet-routing-comparison.csv";
  std::string protocolName = "AODV";

  CommandLine cmd(__FILE__);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distance", "Maximum distance between nodes (m)", distance);
  cmd.AddValue("numNodes", "Number of nodes", numNodes);
  cmd.AddValue("totalTime", "Total simulation time (s)", totalTime);
  cmd.AddValue("dataStart", "Start time for CBR sources (s)", dataStart);
  cmd.AddValue("dataEnd", "End time for CBR sources (s)", dataEnd);
  cmd.AddValue("txPower", "Transmission power (dBm)", txPower);
  cmd.AddValue("nodeSpeed", "Node speed (m/s)", nodeSpeed);
  cmd.AddValue("areaWidth", "Simulation area width (m)", areaWidth);
  cmd.AddValue("areaHeight", "Simulation area height (m)", areaHeight);
  cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue("enableMobilityTrace", "Enable Mobility Tracing", enableMobilityTrace);
  cmd.AddValue("csvFileName", "Output CSV file name", csvFileName);
  cmd.AddValue("protocolName", "Routing protocol to use (AODV, DSDV, OLSR, DSR)", protocolName);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("CcaMode1Threshold", DoubleValue(-80));
  wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90));

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaWidth) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaHeight) + "]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(nodeSpeed) + "]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install(nodes);

  if (enableMobilityTrace)
    {
      AsciiTraceHelper ascii;
      Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("manet-mobility.tr");
      MobilityModel::EnableAsciiAll(stream);
    }

  InternetStackHelper stack;
  if (protocolName == "AODV")
    {
      AodvHelper aodv;
      stack.SetRoutingHelper(aodv);
    }
  else if (protocolName == "DSDV")
    {
      DsdvHelper dsdv;
      stack.SetRoutingHelper(dsdv);
    }
  else if (protocolName == "OLSR")
    {
      OlsrHelper olsr;
      stack.SetRoutingHelper(olsr);
    }
  else if (protocolName == "DSR")
    {
      DsrHelper dsr;
      stack.SetRoutingHelper(dsr);
      stack.Install(nodes);
      dsr.Install();
    }
  else
    {
      NS_ABORT_MSG("Unknown routing protocol: " << protocolName);
    }

  if (protocolName != "DSR")
    {
      stack.Install(nodes);
    }

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8000;
  ApplicationContainer cbrApps;
  ApplicationContainer sinkApps;

  for (int i = 0; i < 10; ++i)
    {
      uint32_t src = rand() % numNodes;
      uint32_t dst = rand() % numNodes;
      while (src == dst)
        {
          dst = rand() % numNodes;
        }

      InetSocketAddress sinkSocket(inInterfaces.GetAddress(dst), sinkPort);
      PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkSocket);
      sinkApps.Add(sinkHelper.Install(nodes.Get(dst)));

      OnOffHelper onoff("ns3::UdpSocketFactory", sinkSocket);
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("256kbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(512));

      double startTime = UniformVariable().GetValue(dataStart, dataEnd);
      cbrApps.Add(onoff.Install(nodes.Get(src)));
      cbrApps.Get(cbrApps.GetN() - 1)->SetStartTime(Seconds(startTime));
      cbrApps.Get(cbrApps.GetN() - 1)->SetStopTime(Seconds(totalTime));
    }

  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(totalTime));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&ManetRoutingComparison::RxTracer));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
  if (enableFlowMonitor)
    {
      monitor = flowmon.InstallAll();
    }

  Simulator::Stop(Seconds(totalTime));
  Simulator::Run();

  if (enableFlowMonitor)
    {
      monitor->CheckForLostPackets();
      flowmon.SerializeToXmlFile(csvFileName, false, false);
    }

  std::ofstream outFile(csvFileName, std::ios_base::app);
  if (!outFile.fail())
    {
      outFile << protocolName << "," << numNodes << "," << nodeSpeed << "," << txPower << "," << g_totalRx << "\n";
      outFile.close();
    }

  Simulator::Destroy();
  return 0;
}