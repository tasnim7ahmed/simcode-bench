#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

class RoutingExperiment
{
public:
  RoutingExperiment();
  void Run(std::string protocol, uint32_t nodeCount, double txp, double areaWidth, double areaHeight, double nodeSpeed, bool flowMonitor, std::string csvFile);
private:
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;

  void SetupMobility(double nodeSpeed, double areaWidth, double areaHeight);
  void SetupRoutingProtocol(std::string protocol);
  void SetupApplications(uint32_t nSinks);
  void SetupTracing();

  double m_txp;
  double m_areaWidth;
  double m_areaHeight;
};

RoutingExperiment::RoutingExperiment()
{
}

void
RoutingExperiment::SetupMobility(double nodeSpeed, double areaWidth, double areaHeight)
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaWidth) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaHeight) + "]"));

  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(nodeSpeed) + "]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install(nodes);

  // Enable ASCII trace for mobility (optional)
  AsciiTraceHelper ascii;
  mobility.EnableAsciiAll(ascii.CreateFileStream("manet-routing-comparison.mob"));
}

void
RoutingExperiment::SetupRoutingProtocol(std::string protocol)
{
  InternetStackHelper stack;
  Ipv4ListRoutingHelper listRH;

  if (protocol == "AODV")
    {
      AodvHelper aodv;
      listRH.Add(aodv, 10);
    }
  else if (protocol == "DSDV")
    {
      DsdvHelper dsdv;
      listRH.Add(dsdv, 10);
    }
  else if (protocol == "DSR")
    {
      DsrHelper dsr;
      DsrMainHelper dsrMain(dsr);
      dsrMain.Install(dsr, nodes);
      return; // DSR already installed
    }
  else if (protocol == "OLSR")
    {
      OlsrHelper olsr;
      listRH.Add(olsr, 10);
    }
  else
    {
      NS_FATAL_ERROR("Unknown routing protocol: " << protocol);
    }

  stack.SetRoutingHelper(listRH);
  stack.Install(nodes);
}

void
RoutingExperiment::SetupApplications(uint32_t nSinks)
{
  uint16_t port = 9;
  ApplicationContainer sinkApps;
  ApplicationContainer sourceApps;

  for (uint32_t i = 0; i < nSinks; ++i)
    {
      uint32_t src = rand() % nodes.GetN();
      uint32_t dst = rand() % nodes.GetN();
      while (dst == src)
        {
          dst = rand() % nodes.GetN();
        }

      // UDP Sink on destination node
      PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      sinkApps.Add(sink.Install(nodes.Get(dst)));

      // OnOff Source on source node
      OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(dst), port)));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(512));

      sourceApps.Add(onoff.Install(nodes.Get(src)));
    }

  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(200.0));

  sourceApps.Start(Seconds(50.0 + 1.0 * rand() / RAND_MAX)); // Between 50 and 51 seconds
  sourceApps.Stop(Seconds(200.0));
}

void
RoutingExperiment::SetupTracing()
{
  Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/Tx", MakeCallback([](Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface){
    static uint64_t txPackets = 0;
    txPackets++;
    std::ofstream of("tx_stats.csv", std::ios::app);
    of << Simulator::Now().GetSeconds() << "," << txPackets << std::endl;
  }));

  Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/Rx", MakeCallback([](Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface){
    static uint64_t rxPackets = 0;
    rxPackets++;
    std::ofstream of("rx_stats.csv", std::ios::app);
    of << Simulator::Now().GetSeconds() << "," << rxPackets << std::endl;
  }));
}

void
RoutingExperiment::Run(std::string protocol, uint32_t nodeCount, double txp, double areaWidth, double areaHeight, double nodeSpeed, bool flowMonitor, std::string csvFile)
{
  m_txp = txp;
  m_areaWidth = areaWidth;
  m_areaHeight = areaHeight;

  nodes.Create(nodeCount);

  // WiFi configuration
  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211a);
  wifiHelper.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  wifiPhy.Set("TxPowerStart", DoubleValue(txp));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txp));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("RxNoiseFigure", DoubleValue(7));

  devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

  SetupRoutingProtocol(protocol);

  if (protocol != "DSR")
    {
      Ipv4AddressHelper address;
      address.SetBase("10.0.0.0", "255.255.255.0");
      interfaces = address.Assign(devices);
    }

  SetupMobility(nodeSpeed, areaWidth, areaHeight);
  SetupApplications(10); // 10 source/sink pairs

  SetupTracing();

  if (flowMonitor)
    {
      FlowMonitorHelper flowmon;
      Ptr<FlowMonitor> monitor = flowmon.InstallAll();
      Simulator::Stop(Seconds(200.0));
      Simulator::Run();

      monitor->CheckForLostPackets();
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
      std::ofstream out(csvFile);
      out.precision(10);
      out << "FlowID,SrcAddr,DstAddr,PktSent,PktRecv,Throughput(Mbps),DelayMean(ms),JitterMean(ms),HopCount" << std::endl;

      std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          out << i->first << ","
              << t.sourceAddress << ","
              << t.destinationAddress << ","
              << i->second.txPackets << ","
              << i->second.rxPackets << ","
              << (i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0) << ","
              << (i->second.delaySum.GetSeconds() / i->second.rxPackets * 1000) << ","
              << (i->second.jitterSum.GetSeconds() / (i->second.rxPackets > 1 ? i->second.rxPackets - 1 : 1) * 1000) << ","
              << (double)i->second.timesForwarded / i->second.rxPackets + 1 << std::endl;
        }
    }
  else
    {
      Simulator::Stop(Seconds(200.0));
      Simulator::Run();
    }

  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  std::string protocol = "AODV"; // Default
  uint32_t nodeCount = 50;
  double txp = 7.5;
  double areaWidth = 300.0;
  double areaHeight = 1500.0;
  double nodeSpeed = 20.0;
  bool flowMonitor = true;
  std::string csvFile = "manet-results.csv";

  CommandLine cmd(__FILE__);
  cmd.AddValue("protocol", "Routing protocol to use (AODV, DSDV, OLSR, DSR)", protocol);
  cmd.AddValue("nodeCount", "Number of nodes in the simulation", nodeCount);
  cmd.AddValue("txp", "Transmission power in dBm", txp);
  cmd.AddValue("areaWidth", "Width of the simulation area", areaWidth);
  cmd.AddValue("areaHeight", "Height of the simulation area", areaHeight);
  cmd.AddValue("nodeSpeed", "Node speed in m/s", nodeSpeed);
  cmd.AddValue("flowMonitor", "Enable flow monitoring", flowMonitor);
  cmd.AddValue("csvFile", "CSV file for output", csvFile);
  cmd.Parse(argc, argv);

  RoutingExperiment experiment;
  experiment.Run(protocol, nodeCount, txp, areaWidth, areaHeight, nodeSpeed, flowMonitor, csvFile);

  return 0;
}