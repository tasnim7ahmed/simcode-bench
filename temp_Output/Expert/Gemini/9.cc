#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  devices[2] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
  devices[3] = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = address.Assign(devices[0]);
  interfaces[1] = address.Assign(devices[1]);
  interfaces[2] = address.Assign(devices[2]);
  interfaces[3] = address.Assign(devices[3]);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps[4];
  serverApps[0] = echoServer.Install(nodes.Get(1));
  serverApps[1] = echoServer.Install(nodes.Get(2));
  serverApps[2] = echoServer.Install(nodes.Get(3));
  serverApps[3] = echoServer.Install(nodes.Get(0));

  for (int i = 0; i < 4; ++i) {
    serverApps[i].Start(Seconds(1.0));
    serverApps[i].Stop(Seconds(10.0));
  }

  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[4];
  clientApps[0] = echoClient.Install(nodes.Get(0));

  echoClient.SetRemote(interfaces[1].GetAddress(1),9);
  clientApps[0].Start(Seconds(2.0));
  clientApps[0].Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(interfaces[1].GetAddress(1), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2[4];
  clientApps2[0] = echoClient2.Install(nodes.Get(1));

  echoClient2.SetRemote(interfaces[2].GetAddress(1),9);
  clientApps2[0].Start(Seconds(3.0));
  clientApps2[0].Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient3(interfaces[2].GetAddress(1), 9);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps3[4];
  clientApps3[0] = echoClient3.Install(nodes.Get(2));

  echoClient3.SetRemote(interfaces[3].GetAddress(1),9);
  clientApps3[0].Start(Seconds(4.0));
  clientApps3[0].Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient4(interfaces[3].GetAddress(1), 9);
  echoClient4.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient4.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps4[4];
  clientApps4[0] = echoClient4.Install(nodes.Get(3));

  echoClient4.SetRemote(interfaces[0].GetAddress(1),9);
  clientApps4[0].Start(Seconds(5.0));
  clientApps4[0].Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("ring_network.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 30, 10);
  anim.SetConstantPosition(nodes.Get(2), 50, 10);
  anim.SetConstantPosition(nodes.Get(3), 70, 10);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}