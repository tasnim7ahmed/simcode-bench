#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/network-module.h"
#include <iostream>
#include <string>

using namespace ns3;

int main(int argc, char* argv[]) {
  bool useIpv6 = false;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("useIpv6", "Set to true to use IPv6", useIpv6);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
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
  Ipv6AddressHelper address6;
  Ipv4InterfaceContainer interfaces;
  Ipv6InterfaceContainer interfaces6;

  if (useIpv6) {
    address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    interfaces6 = address6.Assign(devices);
    for (uint32_t i = 0; i < interfaces6.GetN(); ++i) {
      interfaces6.GetAddress(i, 1);
    }
  } else {
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  uint16_t echoPort = 9;

  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(
      useIpv6 ? interfaces6.GetAddress(1, 1) : interfaces.GetAddress(1),
      echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Schedule(Seconds(0.1), []() {
    if (LogComponentIsEnabled("CsmaChannel")) {
      NS_LOG_INFO("Starting Simulation");
    }
  });

  csma.EnableQueueDisc(
      "ns3::FifoQueueDisc", "MaxPackets", UintegerValue(20), "queue-drop-csma");
  csma.EnablePcapAll("udp-echo", false);

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}