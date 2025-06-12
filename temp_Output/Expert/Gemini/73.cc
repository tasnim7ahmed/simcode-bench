#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer routers;
  routers.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer r0r1 = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer r1r2 = p2p.Install(routers.Get(1), routers.Get(2));

  InternetStackHelper internet;
  internet.Install(routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("x.x.x.0", "255.255.255.252");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(r0r1);

  ipv4.SetBase("y.y.y.0", "255.255.255.252");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(r1r2);

  Ipv4AddressHelper ipv4_loopback;
  ipv4_loopback.SetBase("a.a.a.0", "255.255.255.0");
  NetDeviceContainer loopback0 = LoopbackNetDeviceHelper::CreateNetDevice (routers.Get(0));
  NetDeviceContainer loopback2 = LoopbackNetDeviceHelper::CreateNetDevice (routers.Get(2));
  Ipv4InterfaceContainer i0_loop = ipv4_loopback.Assign (loopback0);
  Ipv4InterfaceContainer i2_loop = ipv4_loopback.Assign (loopback2);
  Ipv4Address addrA = i0_loop.GetAddress (0);
  Ipv4Address addrC = i2_loop.GetAddress (0);
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(addrC, port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer apps = onoff.Install(routers.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                         Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  apps = sink.Install(routers.Get(2));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("router", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}