#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char* argv[]) {
  bool verbose = false;
  bool useIpv6 = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
  cmd.AddValue("useIpv6", "Use IPv6 addresses if true", useIpv6);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv6AddressHelper address6;
  address6.SetBase("2001:db8::", Ipv6Prefix(64));

  Ipv4InterfaceContainer interfaces;
  Ipv6InterfaceContainer interfaces6;
  if (useIpv6) {
    interfaces6 = address6.Assign(devices);
  } else {
    interfaces = address.Assign(devices);
  }

  UdpServerHelper server(4000);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client;
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.01")));
  client.SetAttribute("PacketSize", UintegerValue(1472));
  if (useIpv6) {
    client.SetAttribute("RemoteAddress", AddressValue(interfaces6.GetAddress(1)));
  } else {
    client.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(1)));
  }
  client.SetAttribute("RemotePort", UintegerValue(4000));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}