#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/tcp-westwood.h"
#include "ns3/tcp-illinois.h"
#include "ns3/tcp-scalable.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpOver80211nSimulation");

class ThroughputMonitor : public Object {
public:
  static TypeId GetTypeId(void) {
    return TypeId("ThroughputMonitor")
      .SetParent<Object>()
      .AddConstructor<ThroughputMonitor>();
  }

  ThroughputMonitor(Ptr<OutputStreamWrapper> stream, Time interval)
    : m_stream(stream), m_interval(interval), m_bytesReceived(0) {}

  void TraceSink(Ptr<const Packet> packet, const Address &from) {
    m_bytesReceived += packet->GetSize();
  }

  void Report() {
    double throughput = (m_bytesReceived * 8.0) / (m_interval.GetSeconds() * 1000000);
    *m_stream->GetStream() << Simulator::Now().GetSeconds() << " " << throughput << std::endl;
    m_bytesReceived = 0;
    Simulator::Schedule(m_interval, &ThroughputMonitor::Report, this);
  }

private:
  Ptr<OutputStreamWrapper> m_stream;
  Time m_interval;
  uint64_t m_bytesReceived;
};

int main(int argc, char *argv[]) {
  std::string tcpCCAlgorithm = "TcpNewReno";
  DataRate dataRate("1Mbps");
  uint32_t payloadSize = 1448;
  DataRate wifiBitrate("65Mbps");
  double simulationTime = 10.0;
  bool pcapEnabled = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("tcpCCAlgorithm", "TCP congestion control algorithm (TcpNewReno, TcpWestwood, TcpIllinois, TcpScalable)", tcpCCAlgorithm);
  cmd.AddValue("dataRate", "Application data rate", dataRate);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("wifiBitrate", "Wi-Fi physical layer bitrate", wifiBitrate);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapEnabled);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpCCAlgorithm)));

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(wifiBitrate.GetMode()), "ControlMode", StringValue("HtMcs0"));

  Ssid ssid = Ssid("ns3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign(apDevices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(apInterfaces.GetAddress(0), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("MaxBytes", UintegerValue(0));
  source.SetAttribute("SendSize", UintegerValue(payloadSize));
  ApplicationContainer sourceApps = source.Install(wifiStaNode.Get(0));
  sourceApps.Start(Seconds(0.0));
  sourceApps.Stop(Seconds(simulationTime));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> throughputStream = asciiTraceHelper.CreateFileStream("throughput.dat");
  ThroughputMonitor monitor(throughputStream, MilliSeconds(100));
  Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/Rx", MakeCallback(&ThroughputMonitor::TraceSink, &monitor));
  Simulator::Schedule(MilliSeconds(100), &ThroughputMonitor::Report, &monitor);

  if (pcapEnabled) {
    phy.EnablePcap("tcp_over_80211n", apDevices.Get(0));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}