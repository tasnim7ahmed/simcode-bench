#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingUdpEcho");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("RingUdpEcho", LOG_LEVEL_INFO);

  // Create Nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Configure PointToPoint links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create NetDevices and Channels
  NetDeviceContainer devices[4];
  for (int i = 0; i < 4; ++i) {
    devices[i] = p2p.Install(nodes.Get(i), nodes.Get((i + 1) % 4));
  }

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  for (int i = 0; i < 4; ++i) {
    interfaces[i] = ipv4.Assign(devices[i]);
    ipv4.NewNetwork();
  }

  // Create UdpEchoServer application on each node
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps[4];
  for (int i = 0; i < 4; ++i) {
    serverApps[i] = echoServer.Install(nodes.Get(i));
    serverApps[i].Start(Seconds(0.0));
    serverApps[i].Stop(Seconds(10.0));
  }

  // Create UdpEchoClient application between nodes
  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(5.0));

  UdpEchoClientHelper echoClient1(interfaces[1].GetAddress(1), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
  clientApp1.Start(Seconds(6.0));
  clientApp1.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}