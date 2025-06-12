#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1));

  NetDeviceContainer d02 = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  p2p.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign(d02);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t cbrPort = 12345;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(i23.GetAddress(1), cbrPort)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(210));
  onoff.SetAttribute("DataRate", StringValue("448kbps"));
  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(1.5));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
  apps = sink.Install(nodes.Get(3));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(1.5));

  cbrPort = 12346;
  OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(i12.GetAddress(1), cbrPort)));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute("PacketSize", UintegerValue(210));
  onoff2.SetAttribute("DataRate", StringValue("448kbps"));
  apps = onoff2.Install(nodes.Get(3));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(1.5));

  PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
  apps = sink2.Install(nodes.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(1.5));

  uint16_t ftpPort = 21;
  BulkSendHelper ftp("ns3::TcpSocketFactory", InetSocketAddress(i23.GetAddress(1), ftpPort));
  ftp.SetAttribute("SendSize", UintegerValue(1024));
  apps = ftp.Install(nodes.Get(0));
  apps.Start(Seconds(1.2));
  apps.Stop(Seconds(1.35));

  PacketSinkHelper sink3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
  apps = sink3.Install(nodes.Get(3));
  apps.Start(Seconds(1.2));
  apps.Stop(Seconds(1.35));

  Simulator::Stop(Seconds(2.0));

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}