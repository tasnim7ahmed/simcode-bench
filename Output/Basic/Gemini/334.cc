#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdp");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer staNodes;
  staNodes.Create(1);
  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator", "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"), "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNodes);

  MobilityHelper apMobility;
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.Install(apNode);

  InternetStackHelper internet;
  internet.Install(staNodes);
  internet.Install(apNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeIface = ipv4.Assign(staDevices);
  Ipv4InterfaceContainer apNodeIface = ipv4.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(apNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time interPacketInterval = Seconds(1.0);
  UdpClientHelper client(apNodeIface.GetAddress(0), port);
  client.SetTotalBytes(packetSize * numPackets);
  client.SetMaxPackets(numPackets);
  client.SetPacketSize(packetSize);
  client.SetInterPacketInterval(interPacketInterval);
  ApplicationContainer clientApps = client.Install(staNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}