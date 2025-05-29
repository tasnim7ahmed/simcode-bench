#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(7);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d0d2 = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d1d2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("1Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer d2d3d4d5 = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(3), nodes.Get(4), nodes.Get(5)));

  NetDeviceContainer d5d6 = p2p.Install(nodes.Get(5), nodes.Get(6));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3i4i5 = address.Assign(d2d3d4d5);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i5i6 = address.Assign(d5d6);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(i5i6.GetAddress(1), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(50));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("300bps")));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(6));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Simulator::Schedule(Seconds(0.00001), [](){
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/TxQueue/Enqueue", MakeCallback(&Queue::TraceEnqueue));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/TxQueue/Dequeue", MakeCallback(&Queue::TraceDequeue));
  });

  p2p.EnablePcapAll("mixed_network");
  csma.EnablePcapAll("mixed_network", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}