#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/trace-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd;
  cmd.AddValue("useIpv6", "Set to true to use IPv6 addressing.", useIpv6);
  cmd.AddValue("enableLogging", "Enable logging to file.", enableLogging);
  cmd.Parse(argc, argv);

  if (enableLogging) {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
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
  }
  
  if (!useIpv6)
  {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  }

  UdpClientServerHelper echoClientServer(9, "input.txt");
  echoClientServer.SetAttribute("MaxPackets", UintegerValue(10000));
  echoClientServer.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClientServer.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer serverApp = echoClientServer.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  ApplicationContainer clientApp = echoClientServer.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  if (enableLogging) {
     AsciiTraceHelper ascii;
     csma.EnableAsciiAll (ascii.CreateFileStream ("csma-trace.tr"));
     csma.EnablePcapAll ("csma-pcap");
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}