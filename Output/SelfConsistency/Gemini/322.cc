#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(Vector(20, 0, 0));
  }

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(nodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}