#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopology");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  NodeContainer hubNode;
  hubNode.Create(1);

  NodeContainer spokeNodes;
  spokeNodes.Create(8);

  InternetStackHelper stack;
  stack.Install(hubNode);
  stack.Install(spokeNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (int i = 0; i < 8; ++i) {
    NodeContainer link;
    link.Add(hubNode.Get(0));
    link.Add(spokeNodes.Get(i));

    NetDeviceContainer devices = p2p.Install(link);
    hubDevices.Add(devices.Get(0));
    spokeDevices.Add(devices.Get(1));
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces = address.Assign(hubDevices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer spokeInterfaces;
  for (int i = 0; i < 8; ++i) {
    NetDeviceContainer temp;
    temp.Add(spokeDevices.Get(i));

    Ipv4InterfaceContainer interface = address.Assign(temp);
    spokeInterfaces.Add(interface.Get(0));
    address.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(hubInterfaces.GetAddress(0), port));
  ApplicationContainer sinkApp = sink.Install(hubNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  for (int i = 0; i < 8; ++i) {
    OnOffHelper onoff("ns3::TcpSocketFactory",
                       InetSocketAddress(hubInterfaces.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("14kbps"), 137);

    ApplicationContainer clientApp = onoff.Install(spokeNodes.Get(i));
    clientApp.Start(Seconds(2.0 + i * 0.1));
    clientApp.Stop(Seconds(10.0));
  }

  p2p.EnablePcapAll("star");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}