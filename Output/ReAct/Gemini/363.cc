#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  Address sinkLocalAddress(InetSocketAddress{interfaces.GetAddress(1), port});
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper onOffHelper("ns3::TcpSocketFactory",
                          Address(InetSocketAddress{interfaces.GetAddress(1), port}));
  onOffHelper.SetConstantRate(DataRate("50Mbps"));
  ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}