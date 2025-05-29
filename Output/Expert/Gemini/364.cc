#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/ipv6-address.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

  Icmpv6EchoClientHelper echoClient;
  echoClient.SetRemote(interfaces.GetAddress(1, 1));
  echoClient.SetIfIndex(1);

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(9.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}