#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

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
  for (int i = 0; i < 4; ++i) {
    devices[i] = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % 4));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  for (int i = 0; i < 4; ++i) {
    interfaces[i] = address.Assign(devices[i]);
  }

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps[4];
  for (int i = 0; i < 4; ++i) {
    serverApps[i] = echoServer.Install(nodes.Get(i));
    serverApps[i].Start(Seconds(0.0));
    serverApps[i].Stop(Seconds(5.0));
  }

  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[4];
  clientApps[0] = echoClient.Install(nodes.Get(1));
  clientApps[0].Start(Seconds(1.0));
  clientApps[0].Stop(Seconds(5.0));

  UdpEchoClientHelper echoClient1(interfaces[1].GetAddress(1), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

  clientApps[1] = echoClient1.Install(nodes.Get(2));
  clientApps[1].Start(Seconds(1.1));
  clientApps[1].Stop(Seconds(5.0));

    UdpEchoClientHelper echoClient2(interfaces[2].GetAddress(1), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  clientApps[2] = echoClient2.Install(nodes.Get(3));
  clientApps[2].Start(Seconds(1.2));
  clientApps[2].Stop(Seconds(5.0));

    UdpEchoClientHelper echoClient3(interfaces[3].GetAddress(1), 9);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));

  clientApps[3] = echoClient3.Install(nodes.Get(0));
  clientApps[3].Start(Seconds(1.3));
  clientApps[3].Stop(Seconds(5.0));

  Simulator::Run(Seconds(5.0));
  Simulator::Destroy();
  return 0;
}