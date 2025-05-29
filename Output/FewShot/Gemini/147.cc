#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

  uint16_t port = 9;

  UdpEchoServerHelper echoServer1(port);
  ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(1));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer2(port);
  ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(interfaces01.GetAddress(1), port);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(interfaces02.GetAddress(1), port);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(0));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));


  pointToPoint.EnablePcapAll("branch");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}