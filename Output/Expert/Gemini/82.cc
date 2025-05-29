#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  bool ipv6 = false;
  cmd.AddValue("ipv6", "Use IPv6 rather than IPv4", ipv6);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(20));

  NetDeviceContainer devices;
  for (uint32_t i = 0; i < 3; ++i) {
    NetDeviceContainer link = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
    devices.Add(link.Get(0));
    devices.Add(link.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv6AddressHelper address6;
  if (ipv6) {
    address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  } else {
    address.SetBase("10.1.1.0", "255.255.255.0");
  }

  Ipv4InterfaceContainer interfaces;
  Ipv6InterfaceContainer interfaces6;

  for (uint32_t i = 0; i < 3; ++i) {
    if (ipv6) {
      NetDeviceContainer link;
      link.Add(devices.Get(2 * i));
      link.Add(devices.Get(2 * i + 1));
      interfaces6.Add(address6.Assign(link));
      address6.NewNetwork();

    } else {
      NetDeviceContainer link;
      link.Add(devices.Get(2 * i));
      link.Add(devices.Get(2 * i + 1));
      interfaces.Add(address.Assign(link));
      address.NewNetwork();
    }
  }

  if (!ipv6) {
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  } else {
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();
  }

  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ipv6 ? interfaces6.GetAddress(2, 0) : interfaces.GetAddress(2, 0), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll("udp-echo.tr");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}