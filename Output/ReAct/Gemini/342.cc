#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(20.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->Bind();
  ns3TcpSocket->Connect(sinkAddress);

  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("tcp_trace.txt", std::ios::out);
  ns3TcpSocket->TraceConnectWithoutContext("Tx", "OutputTcpSocketTrace", stream);

  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(512));
  source.SetAttribute("MaxBytes", UintegerValue(51200));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}