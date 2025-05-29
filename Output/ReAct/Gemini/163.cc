#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpNonPersistent");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetPrintLinePrefix(true);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(5.0));

  uint32_t packetSize = 1024;
  uint32_t maxBytes = 2048;
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  onOffHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));

  ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(4.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("tcp-non-persistent.xml");
  anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(nodes.Get(1), 100.0, 10.0);

  Simulator::Stop(Seconds(5.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}