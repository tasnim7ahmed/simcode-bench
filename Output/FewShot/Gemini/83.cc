#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internetv6-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool useIpv6 = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("useIpv6", "Set to true to use IPv6 addresses", useIpv6);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  if (useIpv6) {
    stack.Install(nodes);
  } else {
    stack.Install(nodes);
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ipv6AddressHelper address6;
  address6.SetBase("2001:db8::", Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces6 = address6.Assign(devices);
  interfaces6.SetForwarding(0, true);
  interfaces6.SetForwarding(1, true);
  interfaces6.SetDefaultRouteInAllNodes();

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(320));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}