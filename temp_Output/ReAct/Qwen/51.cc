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

class ThroughputSink : public Application
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ThroughputSink")
      .SetParent<Application>()
      .AddConstructor<ThroughputSink>();
    return tid;
  }

  ThroughputSink() : m_totalRx(0) {}
  virtual ~ThroughputSink() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
  }

  void DoDispose() override
  {
    Application::DoDispose();
  }

  void StartApplication() override
  {
    if (m_socket == nullptr)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&ThroughputSink::Receive, this));
    }
  }

  void StopApplication() override
  {
    if (m_socket != nullptr)
    {
      m_socket->Close();
    }
  }

private:
  void Receive(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      m_totalRx += packet->GetSize();
    }
  }

  uint64_t m_totalRx;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  std::string tcpVariant = "TcpNewReno";
  DataRate dataRate("10Mbps");
  uint32_t payloadSize = 1448;
  Time interval = MilliSeconds(100);
  double simulationTime = 10.0;
  bool pcapTracing = false;
  DataRate phyRate("65Mbps");

  CommandLine cmd(__FILE__);
  cmd.AddValue("tcpVariant", "TCP congestion control algorithm (e.g., TcpNewReno, TcpCubic)", tcpVariant);
  cmd.AddValue("dataRate", "Application data rate", dataRate);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);
  cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpVariant)));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate" + phyRate.GetBitRate() / 1000000 + "MbpsBW20MHz"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(1500));

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
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate" + phyRate.GetBitRate() / 1000000 + "MbpsBW20MHz"), "ControlMode", StringValue("OfdmRate6Mbps"));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("ns3-ssid")),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("ns3-ssid")));
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

  uint16_t port = 50000;
  ThroughputSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(wifiApNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime));

  OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(dataRate));
  onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer app = onoff.Install(wifiStaNode.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(simulationTime - 0.1));

  if (pcapTracing)
  {
    phy.EnablePcap("sta", staDevices.Get(0));
    phy.EnablePcap("ap", apDevices.Get(0));
  }

  Simulator::Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
  {
    Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier())->FindFlow(iter->first);
    if (t.sourcePort == port)
    {
      std::cout << "Flow " << iter->first << " (" << t.destinationAddress << " -> " << t.sourceAddress << ")\n";
      std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
      std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps\n";
    }
  }

  Simulator::Destroy();
  return 0;
}