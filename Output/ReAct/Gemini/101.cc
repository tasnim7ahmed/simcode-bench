#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  InternetStackHelper stack;
  stack.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer d02 = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p2.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));
  NetDeviceContainer d23 = p2p2.Install(nodes.Get(2), nodes.Get(3));

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = address.Assign(d02);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = address.Assign(d12);
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = address.Assign(d23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpServerHelper server0(9);
  ApplicationContainer apps0 = server0.Install(nodes.Get(3));
  apps0.Start(Seconds(1.0));
  apps0.Stop(Seconds(2.0));

  UdpClientHelper client0(i23.GetAddress(1), 9);
  client0.SetAttribute("MaxPackets", UintegerValue(1000000));
  client0.SetAttribute("Interval", TimeValue(Time("2000us")));
  client0.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer apps1 = client0.Install(nodes.Get(0));
  apps1.Start(Seconds(1.0));
  apps1.Stop(Seconds(2.0));

  UdpServerHelper server1(10);
  ApplicationContainer apps2 = server1.Install(nodes.Get(1));
  apps2.Start(Seconds(1.0));
  apps2.Stop(Seconds(2.0));

  UdpClientHelper client1(i12.GetAddress(1), 10);
  client1.SetAttribute("MaxPackets", UintegerValue(1000000));
  client1.SetAttribute("Interval", TimeValue(Time("2000us")));
  client1.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer apps3 = client1.Install(nodes.Get(3));
  apps3.Start(Seconds(1.0));
  apps3.Stop(Seconds(2.0));

  BulkSendHelper ftp(i23.GetAddress(1), 50000);
  ftp.SetAttribute("SendSize", UintegerValue(210));
  ApplicationContainer ftpApp = ftp.Install(nodes.Get(0));
  ftpApp.Start(Seconds(1.2));
  ftpApp.Stop(Seconds(1.35));

  Simulator::Stop(Seconds(2.0));

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}