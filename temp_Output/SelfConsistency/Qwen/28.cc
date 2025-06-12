#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiPerformanceComparison");

class MixedWifiPerformanceExperiment
{
public:
  MixedWifiPerformanceExperiment();
  void Run(bool enableProtection, bool shortPpduFormat, bool shortSlotTime, uint32_t payloadSize, bool useTcp);

private:
  NodeContainer wifiStaNodes;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
};

MixedWifiPerformanceExperiment::MixedWifiPerformanceExperiment()
{
  // Create nodes
  wifiStaNodes.Create(2); // One b/g station and one HT station
  wifiApNode.Create(1);
}

void
MixedWifiPerformanceExperiment::Run(bool enableProtection,
                                    bool shortPpduFormat,
                                    bool shortSlotTime,
                                    uint32_t payloadSize,
                                    bool useTcp)
{
  // Setup channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  // Set standard to 802.11n for mixed mode support
  phy.Set("ChannelWidth", UintegerValue(20));
  phy.Set("ShortGuardInterval", BooleanValue(shortPpduFormat));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  // Configure ERP protection if enabled
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("ErpOfdmRate24Mbps"),
                               "ControlMode", StringValue("DsssRate11Mbps"));

  // MAC configuration
  WifiMacHelper mac;
  Ssid ssid = Ssid("mixed-wifi-network");

  // AP setup
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2.0)));
  if (enableProtection)
    {
      mac.Set("EnableRts", BooleanValue(true));
    }
  if (shortSlotTime)
    {
      mac.Set("ShortSlotTimeSupported", BooleanValue(true));
    }

  apDevice = wifi.Install(phy, mac, wifiApNode.Get(0));

  // Station setup
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  // Install first station as non-HT (legacy b/g)
  wifi.SetStandard(WIFI_STANDARD_80211a); // a/b/g compatible rates
  staDevices.Add(wifi.Install(phy, mac, wifiStaNodes.Get(0)));

  // Install second station as HT
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  staDevices.Add(wifi.Install(phy, mac, wifiStaNodes.Get(1)));

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  apInterface = address.Assign(apDevice);
  staInterfaces = address.Assign(staDevices);

  // Traffic setup
  uint16_t port = 9;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (useTcp)
    {
      // TCP
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      serverApps = sink.Install(wifiStaNodes.Get(0)); // Server on first STA
      serverApps.Start(Seconds(0.5));
      serverApps.Stop(Seconds(10.0));

      OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

      clientApps = onoff.Install(wifiApNode.Get(0));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(10.0));
    }
  else
    {
      // UDP
      UdpServerHelper server(port);
      serverApps = server.Install(wifiStaNodes.Get(0));
      serverApps.Start(Seconds(0.5));
      serverApps.Stop(Seconds(10.0));

      UdpClientHelper client(staInterfaces.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(payloadSize));

      clientApps = client.Install(wifiApNode.Get(0));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(10.0));
    }

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Flow monitor setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Calculate throughput
  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughput = 0;
  for (auto it = stats.begin(); it != stats.end(); ++it)
    {
      totalThroughput += it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000.0; // Mbps
    }

  std::cout << "Configuration: Protection=" << enableProtection
            << ", ShortPPDU=" << shortPpduFormat
            << ", ShortSlot=" << shortSlotTime
            << ", Payload=" << payloadSize
            << ", Protocol=" << (useTcp ? "TCP" : "UDP")
            << " -> Throughput: " << totalThroughput << " Mbps" << std::endl;

  Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
  bool enableProtection = true;
  bool shortPpduFormat = true;
  bool shortSlotTime = true;
  uint32_t payloadSize = 1472;
  bool useTcp = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("enableProtection", "Enable ERP protection mode", enableProtection);
  cmd.AddValue("shortPpduFormat", "Enable short PPDU format", shortPpduFormat);
  cmd.AddValue("shortSlotTime", "Use short slot time", shortSlotTime);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("useTcp", "Use TCP instead of UDP traffic", useTcp);
  cmd.Parse(argc, argv);

  MixedWifiPerformanceExperiment experiment;
  experiment.Run(enableProtection, shortPpduFormat, shortSlotTime, payloadSize, useTcp);

  return 0;
}