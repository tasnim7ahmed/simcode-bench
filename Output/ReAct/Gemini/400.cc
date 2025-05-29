#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifi");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::SetFilter("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer wifiNodes;
  wifiNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
              BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

  uint16_t port = 9;

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                               InetSocketAddress(interfaces.GetAddress(1), port));
  ApplicationContainer sinkApp = sinkHelper.Install(wifiNodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper sourceHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(interfaces.GetAddress(1), port));
  sourceHelper.SetAttribute("PacketSize", UintegerValue(1024));
  sourceHelper.SetAttribute("SendRate", DataRateValue(1000));

  ApplicationContainer sourceApp = sourceHelper.Install(wifiNodes.Get(0));
  sourceApp.Start(Seconds(2.0));
  sourceApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}