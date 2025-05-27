#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/uinteger.h"

using namespace ns3;

int main(int argc, char* argv[]) {
  bool verbose = false;
  bool useIpv6 = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
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
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(4000);
  ApplicationContainer apps = server.Install(nodes.Get(1));
  apps.Start(Seconds(1));
  apps.Stop(Seconds(10));

  UdpClientHelper client(interfaces.GetAddress(1), 4000);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.00001")));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  apps = client.Install(nodes.Get(0));
  apps.Start(Seconds(2));
  apps.Stop(Seconds(10));

  Simulator::Stop(Seconds(11));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}