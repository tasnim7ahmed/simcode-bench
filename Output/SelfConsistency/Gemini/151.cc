#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaExample");

int main(int argc, char* argv[]) {
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(3);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(Seconds(0.002)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  if (tracing) {
    csma.EnablePcapAll("csma-trace");
  }

  Simulator::Run(Seconds(10.0));
  Simulator::Destroy();
  return 0;
}