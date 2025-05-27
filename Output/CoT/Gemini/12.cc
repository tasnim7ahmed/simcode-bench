#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceFile");

int main(int argc, char* argv[]) {
  bool verbose = false;
  bool useIpv6 = false;
  std::string traceFile = "trace.txt";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
  cmd.AddValue("useIpv6", "Set to true to use IPv6 addressing.", useIpv6);
  cmd.AddValue("traceFile", "The name of the trace file", traceFile);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv6AddressHelper address6;
  Ipv4InterfaceContainer interfaces;
  Ipv6InterfaceContainer interfaces6;

  if (useIpv6) {
    address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    interfaces6 = address6.Assign(devices);
  } else {
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  uint16_t port = 4000;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(useIpv6 ? interfaces6.GetAddress(1) : interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.00001")));
  client.SetAttribute("PacketSize", UintegerValue(1472));
  client.SetAttribute("TraceFile", StringValue(traceFile));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}