#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                            "Bounds", StringValue("100x100"));
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}