#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketLossRateLimitingExample");

void
PacketDropCallback(Ptr<const QueueDiscItem> item)
{
  NS_LOG_UNCOND("Packet dropped at time " << Simulator::Now().GetSeconds()
                << "s, from " << item->GetPacket()->GetUid());
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("PacketLossRateLimitingExample", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Create point-to-point link with low bandwidth and small queue to induce loss
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Use a small queue to induce loss
  pointToPoint.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(10));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install UDP server on node 1
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(5.0));

  // Install UDP client on node 0
  uint32_t maxPacketCount = 1000;
  Time interPacketInterval = MilliSeconds(1); // 1ms, higher than 1Mbps bw
  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
  client.SetAttribute("Interval", TimeValue(interPacketInterval));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(5.0));

  // Set up a QueueDisc (e.g., FIFO), with small limit to visualize drops in NetAnim
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("10p"));
  QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

  // Connect drop event to callback for visualization/logging
  qdiscs.Get(0)->TraceConnectWithoutContext("Drop", MakeCallback(&PacketDropCallback));

  // Set up NetAnim
  AnimationInterface anim("packet-loss-rate-limiting.xml");
  anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
  anim.SetConstantPosition(nodes.Get(1), 60.0, 20.0);

  Simulator::Stop(Seconds(6.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}