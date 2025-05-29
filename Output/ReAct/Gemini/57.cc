#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nSpokes = 8;

  NodeContainer hubNode;
  hubNode.Create(1);

  NodeContainer spokeNodes;
  spokeNodes.Create(nSpokes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (uint32_t i = 0; i < nSpokes; ++i) {
    NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));
    hubDevices.Add(link.Get(0));
    spokeDevices.Add(link.Get(1));
  }

  InternetStackHelper internet;
  internet.Install(hubNode);
  internet.Install(spokeNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer hubInterfaces;
  Ipv4InterfaceContainer spokeInterfaces;
  for (uint32_t i = 0; i < nSpokes; ++i) {
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
    hubInterfaces.Add(interfaces.Get(0));
    spokeInterfaces.Add(interfaces.Get(1));
    address.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(hubInterfaces.GetAddress(0), port));
  ApplicationContainer sinkApp = sink.Install(hubNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper onOff("ns3::TcpSocketFactory", InetSocketAddress(hubInterfaces.GetAddress(0), port));
  onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOff.SetAttribute("PacketSize", UintegerValue(137));
  onOff.SetAttribute("DataRate", StringValue("14kbps"));

  for (uint32_t i = 0; i < nSpokes; ++i) {
    ApplicationContainer clientApp = onOff.Install(spokeNodes.Get(i));
    clientApp.Start(Seconds(2.0 + i * 0.1));
    clientApp.Stop(Seconds(10.0));
  }

  pointToPoint.EnablePcapAll("star");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}