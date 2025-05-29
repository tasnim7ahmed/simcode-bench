#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimple");

int main(int argc, char* argv[]) {
  // Enable logging
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);
  NodeContainer apNode = nodes.Get(0);
  NodeContainer staNode = nodes.Get(1);

  // Create channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Create MAC
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevs = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevs = wifi.Install(phy, mac, staNode);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(apDevs, staDevs));

  // Create UDP server
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(apNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create UDP client
  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(staNode);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}