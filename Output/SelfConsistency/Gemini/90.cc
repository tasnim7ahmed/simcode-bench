#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleErrorModel");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint01;
  pointToPoint01.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint01.SetChannelAttribute("Delay", StringValue("2ms"));
  pointToPoint01.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  PointToPointHelper pointToPoint23;
  pointToPoint23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  pointToPoint23.SetChannelAttribute("Delay", StringValue("10ms"));
  pointToPoint23.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices02 = pointToPoint01.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices12 = pointToPoint01.Install(nodes.Get(1), nodes.Get(2));
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

  // Error Model
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.001));
  em->SetAttribute("Unit", StringValue("packet"));

  Ptr<BurstErrorModel> em2 = CreateObject<BurstErrorModel>();
  em2->SetAttribute("ErrorRate", DoubleValue(0.01));
  em2->SetAttribute("BurstSize", IntegerValue(2)); // Example burst size

  ListErrorModel listErrorModel;
  listErrorModel.SetDrop(11);
  listErrorModel.SetDrop(17);

  devices02.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devices12.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em2));
  devices23.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(&listErrorModel));

  // UDP flow n0 -> n3
  uint16_t port1 = 9;
  OnOffHelper onoff1("ns3::UdpSocketFactory",
                    Address(InetSocketAddress(interfaces23.GetAddress(1), port1)));
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff1.SetAttribute("PacketSize", UintegerValue(210));
  onoff1.SetAttribute("DataRate", StringValue("448Kbps")); // 210 * 8 / 0.00375 = 448000
  ApplicationContainer apps1 = onoff1.Install(nodes.Get(0));
  apps1.Start(Seconds(1.0));
  apps1.Stop(Seconds(1.5));

  // UDP flow n3 -> n1
  uint16_t port2 = 9;
  OnOffHelper onoff2("ns3::UdpSocketFactory",
                    Address(InetSocketAddress(interfaces12.GetAddress(0), port2)));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute("PacketSize", UintegerValue(210));
  onoff2.SetAttribute("DataRate", StringValue("448Kbps")); // 210 * 8 / 0.00375 = 448000
  ApplicationContainer apps2 = onoff2.Install(nodes.Get(3));
  apps2.Start(Seconds(1.0));
  apps2.Stop(Seconds(1.5));

  // TCP flow n0 -> n3
  uint16_t port3 = 50000;
  BulkSendHelper ftp("ns3::TcpSocketFactory",
                      Address(InetSocketAddress(interfaces23.GetAddress(1), port3)));
  ftp.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer ftpApp = ftp.Install(nodes.Get(0));
  ftpApp.Start(Seconds(1.2));
  ftpApp.Stop(Seconds(1.35));

  // Tracing
  pointToPoint01.EnableQueueDiscLogging("simple-error-model-queue.tr", devices02.Get(0));
  pointToPoint01.EnableQueueDiscLogging("simple-error-model-queue.tr", devices12.Get(0));
  pointToPoint23.EnableQueueDiscLogging("simple-error-model-queue.tr", devices23.Get(0));

  // Packet reception tracing
  Simulator::TraceConfig::EnableReceivePackets("simple-error-model.tr", nodes);

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}