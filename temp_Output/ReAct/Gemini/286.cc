#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(5);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("100.0"),
                                "Y", StringValue("100.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
  mobility.Install(nodes);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpClientHelper client(interfaces.GetAddress(4), 4000);
  client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  client.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(20.0));

  UdpServerHelper server(4000);
  ApplicationContainer serverApps = server.Install(nodes.Get(4));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));

  phy.EnablePcapAll("manet-aodv");

  AnimationInterface anim("manet-aodv.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 40, 20);
  anim.SetConstantPosition(nodes.Get(2), 70, 30);
  anim.SetConstantPosition(nodes.Get(3), 20, 50);
  anim.SetConstantPosition(nodes.Get(4), 80, 60);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}