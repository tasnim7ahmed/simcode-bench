#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

class UdpTrafficHelper {
public:
  UdpTrafficHelper(Address address, uint16_t port)
    : m_address(address), m_port(port) {}

  ApplicationContainer Install(NodeContainer nodes) const {
    ApplicationContainer apps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<Node> node = nodes.Get(i);
      Ptr<Socket> socket = Socket::CreateSocket(node, TypeId::LookupByName("ns3::UdpSocketFactory"));
      socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
      socket->Connect(InetSocketAddress(m_address, m_port));

      Ptr<UdpClient> app = CreateObject<UdpClient>();
      app->SetRemote(m_address, m_port);
      app->SetPacketInterval(Seconds(0.1));
      app->SetPacketSize(512);

      node->AddApplication(app);
      apps.Add(app);
    }
    return apps;
  }

private:
  Address m_address;
  uint16_t m_port;
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_DEBUG);

  NodeContainer nodes;
  nodes.Create(10);

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.Set("TxPowerStart", DoubleValue(5));
  phy.Set("TxPowerEnd", DoubleValue(5));
  phy.Set("ChannelNumber", UintegerValue(36));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(9), port));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer apps;
  for (uint32_t i = 0; i < nodes.GetN() - 1; ++i) {
    onoff.SetAttribute("StartTime", TimeValue(Seconds(UniformVariable{}.GetValue(1, 5))));
    apps.Add(onoff.Install(nodes.Get(i)));
  }

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("adhoc-wireless.tr"));
  phy.EnablePcapAll("adhoc-wireless");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(30));
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  double receivedBytes = 0;
  double sentBytes = 0;
  Time totalDelay;
  uint32_t packetDelivered = 0;
  uint32_t packetSent = 0;

  for (auto &flow : stats) {
    auto t = flow.second;
    packetSent += t.txPackets;
    packetDelivered += t.rxPackets;
    totalDelay += t.delaySum;
    receivedBytes += t.rxBytes;
    sentBytes += t.txBytes;
  }

  double pdr = (packetSent > 0) ? static_cast<double>(packetDelivered) / packetSent : 0;
  Time avgDelay = (packetDelivered > 0) ? totalDelay / packetDelivered : Seconds(0);

  NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
  NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay.As(Time::S));

  Simulator::Destroy();
  return 0;
}