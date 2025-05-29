#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

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

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;

  // Server application
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Client applications
  BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
  sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
  sourceHelper.SetAttribute("SendRate", DataRateValue(DataRate("5Mbps")));

  ApplicationContainer sourceApps;
  for (int i = 1; i < 4; ++i) {
    ApplicationContainer clientApp = sourceHelper.Install(nodes.Get(i));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
    sourceApps.Add(clientApp);
  }

  csma.EnablePcapAll("csma-tcp");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}