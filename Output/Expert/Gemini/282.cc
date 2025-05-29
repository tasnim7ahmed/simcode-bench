#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

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

  uint16_t port = 8080;

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
  sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
  sourceHelper.SetAttribute("SendRate", DataRateValue(DataRate("5Mbps")));

  ApplicationContainer sourceApps1 = sourceHelper.Install(nodes.Get(1));
  sourceApps1.Start(Seconds(2.0));
  sourceApps1.Stop(Seconds(10.0));

  ApplicationContainer sourceApps2 = sourceHelper.Install(nodes.Get(2));
  sourceApps2.Start(Seconds(3.0));
  sourceApps2.Stop(Seconds(10.0));

  ApplicationContainer sourceApps3 = sourceHelper.Install(nodes.Get(3));
  sourceApps3.Start(Seconds(4.0));
  sourceApps3.Stop(Seconds(10.0));

  csma.EnablePcap("csma-tcp", devices, true);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}