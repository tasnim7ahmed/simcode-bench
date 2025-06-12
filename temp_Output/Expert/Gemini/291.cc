#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(10);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol("ns3::AodvRoutingProtocol", "AllowedHelloLoss", UintegerValue(2));
  stack.Install(nodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("2s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobility.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(4000);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpClientHelper client(interfaces.GetAddress(0), 4000);
  client.SetAttribute("MaxPackets", UintegerValue(20));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));

  phy.EnablePcapAll("manet-aodv");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance();
  globalRouting->PrintRoutingTableAllAt(Seconds(20), &std::cout);

  monitor->SerializeToXmlFile("manet-aodv.flowmon", false, true);

  Simulator::Destroy();

  return 0;
}