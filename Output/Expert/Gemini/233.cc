#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(6);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(2)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(3)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(4)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(5)));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(apDevices);
  interfaces.Add(address.Assign(staDevices));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(12.0));

  UdpServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Add(echoServer.Install(nodes.Get(2)));
  serverApps.Add(echoServer.Install(nodes.Get(3)));
  serverApps.Add(echoServer.Install(nodes.Get(4)));
  serverApps.Add(echoServer.Install(nodes.Get(5)));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(13.0));

  Simulator::Stop(Seconds(14.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}