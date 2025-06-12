#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/sumo-mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);

  SumoMobilityHelper sumoMobility;
  sumoMobility.setSumoBinary("sumo");
  sumoMobility.setNetfile("vanet.net.xml");
  sumoMobility.setRoutefile("vanet.rou.xml");
  sumoMobility.setStepLength(0.1);
  sumoMobility.setPositionUpdateInterval(0.1);
  sumoMobility.prepare(nodes);
  sumoMobility.assignStreams(0);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("vanet-network");
  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  DsrRoutingHelper dsrRouting;
  InternetStackHelper stack;
  stack.SetRoutingHelper(dsrRouting);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(9));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(80.0));

  UdpClientHelper client(interfaces.GetAddress(9), 9);
  client.SetAttribute("MaxPackets", UintegerValue(100000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(80.0));

  Simulator::Stop(Seconds(80));

  AnimationInterface anim("vanet.xml");
  anim.SetMaxPktsPerTraceFile(1000000);

  sumoMobility.enableRemoteTracing("127.0.0.1", 8081);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}