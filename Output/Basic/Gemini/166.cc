#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdhocSimulation");

int main(int argc, char *argv[]) {
  bool tracing = true;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable or disable packet tracing", tracing);
  cmd.AddValue("packetSize", "Size of packets", packetSize);
  cmd.AddValue("numPackets", "Number of packets", numPackets);
  cmd.AddValue("interval", "Interval between packets", interval);
  cmd.Parse(argc, argv);

  LogComponent::SetLevel(AodvRouting::GetTypeId(), LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create(5);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMovement("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(Vector3D(1,1,0));
  }

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(4), port);
  client.SetPacketSize(packetSize);
  client.SetTotalPackets(numPackets);
  client.SetInterval(Seconds(interval));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));


  if (tracing) {
    phy.EnablePcapAll("aodv-adhoc");
  }

  AnimationInterface anim("aodv-adhoc.xml");
  anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(nodes.Get(1), 30.0, 30.0);
  anim.SetConstantPosition(nodes.Get(2), 50.0, 50.0);
  anim.SetConstantPosition(nodes.Get(3), 70.0, 70.0);
  anim.SetConstantPosition(nodes.Get(4), 90.0, 90.0);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4RoutingTable> routingTable = aodv.GetRoutingTable(nodes.Get(0));
  routingTable->PrintRoutingTable(std::cout);

  routingTable = aodv.GetRoutingTable(nodes.Get(4));
  routingTable->PrintRoutingTable(std::cout);

  flowmon.SerializeToXmlFile("aodv-adhoc.flowmon", false, true);

  Simulator::Destroy();

  return 0;
}