#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint01;
  pointToPoint01.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint01.SetChannelAttribute("Delay", StringValue("2ms"));
  TrafficControlHelper pfifo01;
  uint16_t queueDiscId01 = pfifo01.Install(nodes.Get(2)->GetId());
  pointToPoint01.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices02 = pointToPoint01.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices12 = pointToPoint01.Install(nodes.Get(1), nodes.Get(2));

  PointToPointHelper pointToPoint23;
  pointToPoint23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  pointToPoint23.SetChannelAttribute("Delay", StringValue("10ms"));
  TrafficControlHelper pfifo23;
  uint16_t queueDiscId23 = pfifo23.Install(nodes.Get(2)->GetId());
  pointToPoint23.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices23 = pointToPoint23.Install(nodes.Get(2), nodes.Get(3));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port1 = 9;
  UdpClientHelper client0(interfaces23.GetAddress(1), port1);
  client0.SetAttribute("MaxPackets", UintegerValue(1000000));
  client0.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  client0.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer clientApps0 = client0.Install(nodes.Get(0));
  clientApps0.Start(Seconds(1.0));
  clientApps0.Stop(Seconds(2.0));

  UdpServerHelper server0(port1);
  ApplicationContainer serverApps0 = server0.Install(nodes.Get(3));
  serverApps0.Start(Seconds(0.9));
  serverApps0.Stop(Seconds(2.1));

  uint16_t port2 = 10;
  UdpClientHelper client1(interfaces12.GetAddress(0), port2);
  client1.SetAttribute("MaxPackets", UintegerValue(1000000));
  client1.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  client1.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer clientApps1 = client1.Install(nodes.Get(3));
  clientApps1.Start(Seconds(1.0));
  clientApps1.Stop(Seconds(2.0));

  UdpServerHelper server1(port2);
  ApplicationContainer serverApps1 = server1.Install(nodes.Get(1));
  serverApps1.Start(Seconds(0.9));
  serverApps1.Stop(Seconds(2.1));

  V4PingHelper ping(interfaces23.GetAddress(1));
  ping.SetAttribute("Verbose", BooleanValue(true));
  ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ApplicationContainer pingApps = ping.Install(nodes.Get(0));
  pingApps.Start(Seconds(1.1));
  pingApps.Stop(Seconds(1.9));

  BulkSendHelper ftp(interfaces23.GetAddress(1), 50000);
  ftp.SetAttribute("SendSize", UintegerValue(1400));
  ApplicationContainer ftpApps = ftp.Install(nodes.Get(0));
  ftpApps.Start(Seconds(1.2));
  ftpApps.Stop(Seconds(1.35));

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.001));
  em->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
  devices23.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  Ptr<BurstErrorModel> bem = CreateObject<BurstErrorModel>();
  bem->SetAttribute("ErrorRate", DoubleValue(0.01));
  bem->SetAttribute("BurstSize", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  devices02.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(bem));

  Ptr<ListErrorModel> lem = CreateObject<ListErrorModel>();
  lem->SetDrop(11);
  lem->SetDrop(17);
  devices12.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(lem));

  Simulator::Stop(Seconds(5.0));

  AsciiTraceHelper ascii;
  PointToPointHelper::EnableAsciiAll(ascii.CreateFileStream("simple-error-model.tr"));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}