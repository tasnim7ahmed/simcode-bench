#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create a TcpServer application on node 0
  uint16_t port = 8080;
  Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(0), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Create a BulkSendApplication on nodes 1, 2, and 3
  BulkSendHelper sourceHelper("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(0), port));
  sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
  sourceHelper.SetAttribute("SendRate",
                            DataRateValue(DataRate("5Mbps"))); // Set send rate to 5 Mbps

  ApplicationContainer sourceApps;
  for (int i = 1; i < 4; ++i) {
    sourceApps.Add(sourceHelper.Install(nodes.Get(i)));
  }
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable PCAP tracing
  csma.EnablePcapAll("csma-tcp", false);

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}