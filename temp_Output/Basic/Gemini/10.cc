#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char* argv[]) {
  bool verbose = false;
  bool tracing = false;
  bool ipv6 = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("ipv6", "Enable IPv6", ipv6);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces;
  Ipv6AddressHelper ipv6Address;
  Ipv6InterfaceContainer ipv6Interfaces;

  if (ipv6) {
    ipv6Address.SetBase(Ipv6Address("2001:db8:0:0:0:0:0:0"),
                        Ipv6Prefix(64));
    ipv6Interfaces = ipv6Address.Assign(devices);
  }

  address.SetBase("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  if (ipv6) {
      UdpEchoClientHelper echoClient6(ipv6Interfaces.GetAddress(1,1), 9);
      echoClient6.SetAttribute("MaxPackets", UintegerValue(10));
      echoClient6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
      echoClient6.SetAttribute("PacketSize", UintegerValue(1024));

      ApplicationContainer clientApps6 = echoClient6.Install(nodes.Get(0));
      clientApps6.Start(Seconds(3.0));
      clientApps6.Stop(Seconds(10.0));
  }

  if (tracing) {
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    csma.EnablePcapAll("udp-echo", true);
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}