#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/log.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer routers;
  routers.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer r0r1 = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer r0r2 = p2p.Install(routers.Get(0), routers.Get(2));
  NetDeviceContainer r1r3 = p2p.Install(routers.Get(1), routers.Get(3));
  NetDeviceContainer r2r3 = p2p.Install(routers.Get(2), routers.Get(3));

  InternetStackHelper internet;
  internet.Install(routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(r0r1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign(r0r2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i3 = ipv4.Assign(r1r3);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign(r2r3);

  OlsrHelper olsr;
  RoutingHelper staticRouting;

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);

  ApplicationContainer serverApps = echoServer.Install(routers.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(i2i3.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(routers.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("lsa-trace");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  AnimationInterface anim("lsa-animation.xml");
  anim.SetConstantPosition(routers.Get(0), 10, 10);
  anim.SetConstantPosition(routers.Get(1), 30, 10);
  anim.SetConstantPosition(routers.Get(2), 10, 30);
  anim.SetConstantPosition(routers.Get(3), 30, 30);


  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance();
  if (globalRouting) {
      globalRouting->PrintRoutingTables(Seconds(11.0));
  }

  monitor->SerializeToXmlFile("lsa-flowmon.xml", true, true);

  Simulator::Destroy();

  return 0;
}