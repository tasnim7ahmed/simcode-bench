#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGridExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLevel(
      "TcpClient", LOG_LEVEL_INFO);  // Enable TCP client logging
  LogComponent::SetLevel(
      "TcpServer", LOG_LEVEL_INFO);  // Enable TCP server logging

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));
  staDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(1)));

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  interfaces.Add(address.Assign(apDevices).Get(0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
                                DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(1.0), "DeltaY",
                                DoubleValue(1.0), "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  uint16_t port = 9;  // Ephemeral port

  // TCP server on node 2
  TcpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  // TCP client on node 0
  TcpClientHelper client0(interfaces.GetAddress(2), port);
  client0.SetAttribute("MaxPackets", UintegerValue(10));
  client0.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client0.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps0 = client0.Install(nodes.Get(0));
  clientApps0.Start(Seconds(2.0));
  clientApps0.Stop(Seconds(20.0));

  // TCP client on node 1
  TcpClientHelper client1(interfaces.GetAddress(2), port);
  client1.SetAttribute("MaxPackets", UintegerValue(10));
  client1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client1.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps1 = client1.Install(nodes.Get(1));
  clientApps1.Start(Seconds(3.0));
  clientApps1.Stop(Seconds(20.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(20.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}