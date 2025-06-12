#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer routers;
  routers.Create(3);
  NodeContainer routerA = NodeContainer(routers.Get(0));
  NodeContainer routerB = NodeContainer(routers.Get(1));
  NodeContainer routerC = NodeContainer(routers.Get(2));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB = p2p.Install(routerA, routerB);
  NetDeviceContainer devicesBC = p2p.Install(routerB, routerC);

  InternetStackHelper internet;
  internet.Install(routers);

  Ipv4AddressHelper address;
  address.SetBase("x.x.x.0", "255.255.255.252");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("y.y.y.0", "255.255.255.252");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  Ipv4AddressHelper addressA;
  addressA.SetBase("a.a.a.0", "255.255.255.255");
  NetDeviceContainer deviceA;
  deviceA.Add(routers.Get(0)->CreateLoopbackNetDevice());
  Ipv4InterfaceContainer interfaceA = addressA.Assign(deviceA);
  Ipv4Address aAddress = interfaceA.GetAddress(0);

  Ipv4AddressHelper addressC;
  addressC.SetBase("c.c.c.0", "255.255.255.255");
  NetDeviceContainer deviceC;
  deviceC.Add(routers.Get(2)->CreateLoopbackNetDevice());
  Ipv4InterfaceContainer interfaceC = addressC.Assign(deviceC);
  Ipv4Address cAddress = interfaceC.GetAddress(0);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer apps = server.Install(routerC.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t maxPackets = 4294967295;
  OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(cAddress, port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  apps = onoff.Install(routerA.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("router");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}