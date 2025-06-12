#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(1)));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(2));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (1.0),
                                 "DeltaY", DoubleValue (1.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices,apDevices));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(20.0));

  OnOffHelper onoff1("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  onoff1.SetAttribute("PacketSize", UintegerValue(512));
  onoff1.SetAttribute("MaxBytes", UintegerValue(5120));

  ApplicationContainer clientApps1 = onoff1.Install(nodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(20.0));

  OnOffHelper onoff2("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  onoff2.SetAttribute("PacketSize", UintegerValue(512));
  onoff2.SetAttribute("MaxBytes", UintegerValue(5120));

  ApplicationContainer clientApps2 = onoff2.Install(nodes.Get(1));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}