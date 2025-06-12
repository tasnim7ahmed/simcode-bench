#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpOver80211n");

class ThroughputMonitor
{
public:
  ThroughputMonitor(Ptr<OutputStreamWrapper> stream, uint64_t intervalMs)
      : m_stream(stream), m_interval(intervalMs), m_bytesReceived(0) {}

  void TraceRx(Ptr<const Packet> packet, const Address &from)
  {
    m_bytesReceived += packet->GetSize();
  }

  void Report()
  {
    double throughput = (m_bytesReceived * 8.0) / (m_interval.GetMilliSeconds() * 1000.0);
    *m_stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput << std::endl;
    m_bytesReceived = 0;
    Simulator::Schedule(MilliSeconds(m_interval.GetMilliSeconds()), &ThroughputMonitor::Report, this);
  }

private:
  Ptr<OutputStreamWrapper> m_stream;
  Time m_interval;
  uint64_t m_bytesReceived;
};

int main(int argc, char *argv[])
{
  std::string congestionControl = "NewReno";
  DataRate dataRate("1000kbps");
  uint32_t payloadSize = 1472;
  Time simulationDuration = Seconds(10);
  bool pcapTracing = false;
  DataRate phyRate("65Mbps");

  CommandLine cmd(__FILE__);
  cmd.AddValue("congestionControl", "TCP congestion control algorithm to use (e.g., NewReno, Cubic)", congestionControl);
  cmd.AddValue("dataRate", "Application data rate", dataRate);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationDuration", "Simulation duration", simulationDuration);
  cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue("phyRate", "Wi-Fi physical layer bitrate", phyRate);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
  Config::SetDefault("ns3::TcpSocketBase::CongestionOps", StringValue("ns3::Tcp" + congestionControl));

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
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyRate.GetMode()), "ControlMode",
                               StringValue(phyRate.GetMode()));

  mac.SetType("ns3::StaWifiMac", "QosSupported", BooleanValue(true));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "QosSupported", BooleanValue(true));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(0.0), "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(apInterfaces.GetAddress(0), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(simulationDuration);

  OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(dataRate));
  onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer sourceApp = onoff.Install(wifiStaNode.Get(0));
  sourceApp.Start(Seconds(0.1));
  sourceApp.Stop(simulationDuration - Seconds(0.1));

  if (pcapTracing)
  {
    phy.EnablePcap("tcp_over_80211n", apDevices.Get(0));
    phy.EnablePcap("tcp_over_80211n", staDevices.Get(0));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> throughputStream = asciiTraceHelper.CreateFileStream("throughput.dat");
  ThroughputMonitor throughputMonitor(throughputStream, 100);
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&ThroughputMonitor::TraceRx, &throughputMonitor));
  Simulator::Schedule(MilliSeconds(100), &ThroughputMonitor::Report, &throughputMonitor);

  Simulator::Stop(simulationDuration);
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
  {
    NS_LOG_INFO("Flow ID: " << iter->first << " "
                            << "Tx Packets = " << iter->second.txPackets << " "
                            << "Rx Packets = " << iter->second.rxPackets << " "
                            << "Throughput: " << iter->second.rxBytes * 8.0 / simulationDuration.GetSeconds() / 1000000 << " Mbps");
  }

  Simulator::Destroy();
  return 0;
}