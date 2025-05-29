#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

  uint16_t port = 50000;

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t maxPackets = 0;
  TcpClientHelper clientHelper(interfaces.GetAddress(1), port);
  clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.1)));

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("wifi-tcp.xml");
  anim.SetConstantPosition(nodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(nodes.Get(1), 10.0, 10.0);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}