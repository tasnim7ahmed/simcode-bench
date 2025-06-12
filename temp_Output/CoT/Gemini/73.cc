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

  NodeContainer routers;
  routers.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer r0r1 = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer r1r2 = p2p.Install(routers.Get(1), routers.Get(2));

  InternetStackHelper internet;
  internet.Install(routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(r0r1);

  ipv4.SetBase("10.1.2.0", "255.255.255.252");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(r1r2);

  Ipv4AddressHelper ipv4_0;
  ipv4_0.SetBase("10.1.3.0", "255.255.255.255");
  Ipv4InterfaceContainer i0 = ipv4_0.Assign(routers.Get(0)->GetDevice(0));

  Ipv4AddressHelper ipv4_2;
  ipv4_2.SetBase("10.1.4.0", "255.255.255.255");
  Ipv4InterfaceContainer i2 = ipv4_2.Assign(routers.Get(2)->GetDevice(0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(i2.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(routers.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkAddress);
  onOffHelper.SetConstantRate(DataRate("500kbps"));
  ApplicationContainer onOffApps = onOffHelper.Install(routers.Get(0));
  onOffApps.Start(Seconds(1.0));
  onOffApps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("router");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}