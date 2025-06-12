#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiPerformanceTest");

class WifiPerformanceExperiment
{
public:
  WifiPerformanceExperiment();
  void Run(uint32_t payloadSize, bool enableErpProtection, bool useShortPpdu, bool useShortSlotTime, std::string trafficType);

private:
  void SetupNetwork(bool enableErpProtection, bool useShortPpdu, bool useShortSlotTime);
  void SetupApplications(uint32_t payloadSize, std::string trafficType);
  void ReportResults(std::string trafficType, uint32_t payloadSize, uint32_t configIndex);

  NodeContainer m_apNode;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevice;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterfaces;
  std::vector<Ptr<FlowMonitor>> m_flowMonitors;
};

WifiPerformanceExperiment::WifiPerformanceExperiment()
{
  m_apNode.Create(1);
  m_staNodes.Create(4); // Two b/g, two HT
}

void WifiPerformanceExperiment::SetupNetwork(bool enableErpProtection, bool useShortPpdu, bool useShortSlotTime)
{
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  // ERP protection mode
  wifi.Set("EnableRts", BooleanValue(enableErpProtection));
  wifi.Set("RtsCtsThreshold", UintegerValue(1500)); // Only if needed

  // Short PPDU format (Greenfield vs Mixed)
  wifi.Set("ShortPlcpPreambleSupported", BooleanValue(useShortPpdu));

  // Short Slot Time
  wifi.Set("ShortSlotTimeSupported", BooleanValue(useShortSlotTime));

  // HT configurations for AP
  Ssid ssid = Ssid("mixed-network");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(2.5)));

  m_apDevice = wifi.Install(phy, mac, m_apNode);

  // Stations: first two as legacy (b/g), next two as HT
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  // Legacy stations (b/g only)
  wifi.SetStandard(WIFI_STANDARD_80211g);
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(0)));
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(1)));

  // HT stations
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(2)));
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(3)));

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNodes);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterface = address.Assign(m_apDevice);
  m_staInterfaces = address.Assign(m_staDevices);
}

void WifiPerformanceExperiment::SetupApplications(uint32_t payloadSize, std::string trafficType)
{
  uint16_t port = 9;

  for (uint32_t i = 0; i < m_staNodes.GetN(); ++i)
  {
    Address sinkAddress(InetSocketAddress(m_apInterface.GetAddress(0), port));
    Ptr<Node> node = m_staNodes.Get(i);
    Ptr<Application> app;

    if (trafficType == "udp")
    {
      OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(m_apInterface.GetAddress(0), port));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      app = onoff.Install(node).Get(0);
    }
    else if (trafficType == "tcp")
    {
      BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(m_apInterface.GetAddress(0), port));
      bulkSend.SetAttribute("MaxBytes", UintegerValue(payloadSize * 1000));
      app = bulkSend.Install(node).Get(0);
    }

    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));
  }

  PacketSinkHelper sinkHelper((trafficType == "udp") ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(m_apNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(11.0));
}

void WifiPerformanceExperiment::ReportResults(std::string trafficType, uint32_t payloadSize, uint32_t configIndex)
{
  FlowMonitorHelper flowmon;
  m_flowMonitors.push_back(flowmon.InstallAll());
  m_flowMonitors.back()->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitors.back()->GetFlowStats();

  std::cout << "\nConfiguration " << configIndex << " - Traffic: " << trafficType
            << ", Payload Size: " << payloadSize
            << ", ERP Protection: " << (configIndex & 0x1 ? "ON" : "OFF")
            << ", Short PPDU: " << ((configIndex >> 1) & 0x1 ? "ON" : "OFF")
            << ", Short Slot Time: " << ((configIndex >> 2) & 0x1 ? "ON" : "OFF")
            << std::endl;

  for (auto it = stats.begin(); it != stats.end(); ++it)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\t";
    std::cout << "Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps\t";
    std::cout << "Loss: " << it->second.packetLossRatio * 100 << "%\n";
  }
}

void WifiPerformanceExperiment::Run(uint32_t payloadSize, bool enableErpProtection, bool useShortPpdu, bool useShortSlotTime, std::string trafficType)
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  SetupNetwork(enableErpProtection, useShortPpdu, useShortSlotTime);
  SetupApplications(payloadSize, trafficType);
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  static uint32_t configIndex = 0;
  ReportResults(trafficType, payloadSize, configIndex++);
  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  uint32_t payloadSize = 1000;
  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.Parse(argc, argv);

  WifiPerformanceExperiment experiment;

  // Test UDP with various configurations
  experiment.Run(payloadSize, true, false, true, "udp");
  experiment.Run(payloadSize, false, true, true, "udp");
  experiment.Run(payloadSize, false, false, false, "udp");

  // Test TCP with various configurations
  experiment.Run(payloadSize, true, true, false, "tcp");
  experiment.Run(payloadSize, false, true, true, "tcp");
  experiment.Run(payloadSize, true, false, false, "tcp");

  return 0;
}