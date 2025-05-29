#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

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

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  TcpClientHelper clientHelper(serverAddress);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(100));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  TcpServerHelper serverHelper(port);
  ApplicationContainer serverApps = serverHelper.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}