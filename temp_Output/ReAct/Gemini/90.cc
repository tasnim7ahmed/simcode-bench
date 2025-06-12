#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ErrorModelExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  NetDeviceContainer devices02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign (devices02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP flow n0 -> n3
  uint16_t port1 = 9;
  UdpClientHelper client1 (interfaces23.GetAddress (1), port1);
  client1.SetAttribute ("MaxPackets", UintegerValue (1500));
  client1.SetAttribute ("Interval", TimeValue (Seconds (0.00375)));
  client1.SetAttribute ("PacketSize", UintegerValue (210));
  ApplicationContainer clientApps1 = client1.Install (nodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (2.0));

  UdpServerHelper server1 (port1);
  ApplicationContainer serverApps1 = server1.Install (nodes.Get (3));
  serverApps1.Start (Seconds (0.9));
  serverApps1.Stop (Seconds (2.1));

  // UDP flow n3 -> n1
  uint16_t port2 = 10;
  UdpClientHelper client2 (interfaces12.GetAddress (0), port2);
  client2.SetAttribute ("MaxPackets", UintegerValue (1500));
  client2.SetAttribute ("Interval", TimeValue (Seconds (0.00375)));
  client2.SetAttribute ("PacketSize", UintegerValue (210));
  ApplicationContainer clientApps2 = client2.Install (nodes.Get (3));
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (2.0));

  UdpServerHelper server2 (port2);
  ApplicationContainer serverApps2 = server2.Install (nodes.Get (1));
  serverApps2.Start (Seconds (0.9));
  serverApps2.Stop (Seconds (2.1));

  // TCP flow n0 -> n3
  uint16_t port3 = 21;
  BulkSendHelper ftp (interfaces23.GetAddress (1), port3);
  ftp.SetAttribute ("SendSize", UintegerValue (1024));
  ApplicationContainer ftpApps = ftp.Install (nodes.Get (0));
  ftpApps.Start (Seconds (1.2));
  ftpApps.Stop (Seconds (1.35));

  PacketSinkHelper sink (port3);
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (1.1));
  sinkApps.Stop (Seconds (1.45));

  // Error Models
  Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel> ();
  em1->SetAttribute ("ErrorRate", DoubleValue (0.001));
  em1->SetAttribute ("RandomVariable", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
  devices23.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));

    Ptr<BurstErrorModel> em2 = CreateObject<BurstErrorModel> ();
  em2->SetAttribute ("ErrorRate", DoubleValue (0.01));
  em2->SetAttribute ("BurstSize", IntegerValue (5));
  devices23.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em2));

  Ptr<ListErrorModel> em3 = CreateObject<ListErrorModel> ();
  em3->SetAttribute ("ErrorList", StringValue ("11,17"));
  devices23.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em3));

  // Tracing
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("simple-error-model.tr"));
  Simulator::Stop (Seconds (3.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}