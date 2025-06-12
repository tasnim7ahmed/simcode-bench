#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/sumo-mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(10);

  SumoMobilityHelper sumo;
  sumo.sumoBinary = "sumo";
  sumo.sumoConfig = "vanet.sumocfg"; // Replace with your SUMO config file
  sumo.Apply(nodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("vanet-network");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  DsrRoutingHelper dsrRouting;
  AodvRoutingHelper aodvRouting;
  OlsrRoutingHelper olsrRouting;
  GlsrRoutingHelper glsrRouting;

  InternetStackHelper stack;
  stack.SetRouting(dsrRouting);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(80.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(5), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(80.0));

  Simulator::Stop(Seconds(80.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}