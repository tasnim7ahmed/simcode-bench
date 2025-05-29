#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/queue.h"
#include "ns3/packet.h"
#include <fstream>

using namespace ns3;

static std::ofstream traceFile;

void
QueueDiscTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  traceFile << Simulator::Now().GetSeconds() << " " << context << " QueueDisc enqueue pkt UID: " << item->GetPacket()->GetUid() << std::endl;
}

void
QueueEnqueueTrace(std::string context, Ptr<const Packet> packet)
{
  traceFile << Simulator::Now().GetSeconds() << " " << context << " Queue enqueue pkt UID: " << packet->GetUid() << std::endl;
}

void
QueueDequeueTrace(std::string context, Ptr<const Packet> packet)
{
  traceFile << Simulator::Now().GetSeconds() << " " << context << " Queue dequeue pkt UID: " << packet->GetUid() << std::endl;
}

void
QueueDropTrace(std::string context, Ptr<const Packet> packet)
{
  traceFile << Simulator::Now().GetSeconds() << " " << context << " Queue drop pkt UID: " << packet->GetUid() << std::endl;
}

void
RxTrace(nstime_t t, std::string path, Ptr<const Packet> p, const Address &a)
{
  traceFile << Simulator::Now().GetSeconds() << " " << path << " Rx pkt UID: " << p->GetUid() << ", size: " << p->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  // Tracing file open
  traceFile.open("simple-error-model.tr", std::ios::out);

  // Logging, if needed
  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4); // n0, n1, n2, n3

  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n2 = nodes.Get(2);
  Ptr<Node> n3 = nodes.Get(3);

  // Point-to-point channels
  // n0<-->n2
  PointToPointHelper p2p_n0n2;
  p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

  // n1<-->n2
  PointToPointHelper p2p_n1n2;
  p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

  // n3<-->n2
  PointToPointHelper p2p_n3n2;
  p2p_n3n2.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p_n3n2.SetChannelAttribute("Delay", StringValue("10ms"));

  // Install devices
  NetDeviceContainer d_n0n2 = p2p_n0n2.Install(n0, n2);
  NetDeviceContainer d_n1n2 = p2p_n1n2.Install(n1, n2);
  NetDeviceContainer d_n3n2 = p2p_n3n2.Install(n3, n2);

  // Set DropTail queues explicitly
  p2p_n0n2.SetQueue("ns3::DropTailQueue");
  p2p_n1n2.SetQueue("ns3::DropTailQueue");
  p2p_n3n2.SetQueue("ns3::DropTailQueue");

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n2 = address.Assign(d_n0n2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n2 = address.Assign(d_n1n2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n3n2 = address.Assign(d_n3n2);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create error models

  // 1. RateErrorModel: 0.001 pkt error rate
  Ptr<RateErrorModel> rateError = CreateObject<RateErrorModel>();
  rateError->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
  rateError->SetRate(0.001);

  // 2. BurstErrorModel: 0.01 error rate
  Ptr<BurstErrorModel> burstError = CreateObject<BurstErrorModel>();
  burstError->SetRate(0.01);

  // 3. ListErrorModel: drop UID 11 & 17
  Ptr<ListErrorModel> listError = CreateObject<ListErrorModel>();
  std::list<uint32_t> list;
  list.push_back(11);
  list.push_back(17);
  listError->SetList(list);

  // Assign error models to n2's device 0,1,2 (3 point-to-point links)
  d_n0n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(rateError));
  d_n1n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(burstError));
  d_n3n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(listError));

  // CBR/UDP from n0 to n3
  uint16_t udpPort1 = 4001;
  UdpServerHelper udpServer1(udpPort1);
  ApplicationContainer serverApps1 = udpServer1.Install(n3);
  serverApps1.Start(Seconds(0.0));
  serverApps1.Stop(Seconds(2.0));

  UdpClientHelper udpClient1(if_n3n2.GetAddress(0), udpPort1);
  udpClient1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  udpClient1.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  udpClient1.SetAttribute("PacketSize", UintegerValue(210));

  ApplicationContainer clientApps1 = udpClient1.Install(n0);
  clientApps1.Start(Seconds(0.1));
  clientApps1.Stop(Seconds(2.0));

  // CBR/UDP from n3 to n1
  uint16_t udpPort2 = 4002;
  UdpServerHelper udpServer2(udpPort2);
  ApplicationContainer serverApps2 = udpServer2.Install(n1);
  serverApps2.Start(Seconds(0.0));
  serverApps2.Stop(Seconds(2.0));

  UdpClientHelper udpClient2(if_n1n2.GetAddress(0), udpPort2);
  udpClient2.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  udpClient2.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  udpClient2.SetAttribute("PacketSize", UintegerValue(210));

  ApplicationContainer clientApps2 = udpClient2.Install(n3);
  clientApps2.Start(Seconds(0.1));
  clientApps2.Stop(Seconds(2.0));

  // FTP/TCP from n0 to n3 (BulkSend)
  uint16_t tcpPort = 5001;
  Address sinkAddress(InetSocketAddress(if_n3n2.GetAddress(0), tcpPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer tcpSinkApp = packetSinkHelper.Install(n3);
  tcpSinkApp.Start(Seconds(0.0));
  tcpSinkApp.Stop(Seconds(2.0));

  BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
  bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
  ApplicationContainer bulkSenderApp = bulkSender.Install(n0);
  bulkSenderApp.Start(Seconds(1.2));
  bulkSenderApp.Stop(Seconds(1.35));

  // Enable tracing of queues for all devices involved
  for (uint32_t i = 0; i < d_n0n2.GetN(); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << d_n0n2.Get(i)->GetNode()->GetId() << "/DeviceList/" << d_n0n2.Get(i)->GetIfIndex() << "/$ns3::PointToPointNetDevice/TxQueue/";
      Config::Connect(oss.str() + "Enqueue", MakeBoundCallback(&QueueEnqueueTrace, oss.str() + "Enqueue"));
      Config::Connect(oss.str() + "Dequeue", MakeBoundCallback(&QueueDequeueTrace, oss.str() + "Dequeue"));
      Config::Connect(oss.str() + "Drop", MakeBoundCallback(&QueueDropTrace, oss.str() + "Drop"));
    }
  for (uint32_t i = 0; i < d_n1n2.GetN(); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << d_n1n2.Get(i)->GetNode()->GetId() << "/DeviceList/" << d_n1n2.Get(i)->GetIfIndex() << "/$ns3::PointToPointNetDevice/TxQueue/";
      Config::Connect(oss.str() + "Enqueue", MakeBoundCallback(&QueueEnqueueTrace, oss.str() + "Enqueue"));
      Config::Connect(oss.str() + "Dequeue", MakeBoundCallback(&QueueDequeueTrace, oss.str() + "Dequeue"));
      Config::Connect(oss.str() + "Drop", MakeBoundCallback(&QueueDropTrace, oss.str() + "Drop"));
    }
  for (uint32_t i = 0; i < d_n3n2.GetN(); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << d_n3n2.Get(i)->GetNode()->GetId() << "/DeviceList/" << d_n3n2.Get(i)->GetIfIndex() << "/$ns3::PointToPointNetDevice/TxQueue/";
      Config::Connect(oss.str() + "Enqueue", MakeBoundCallback(&QueueEnqueueTrace, oss.str() + "Enqueue"));
      Config::Connect(oss.str() + "Dequeue", MakeBoundCallback(&QueueDequeueTrace, oss.str() + "Dequeue"));
      Config::Connect(oss.str() + "Drop", MakeBoundCallback(&QueueDropTrace, oss.str() + "Drop"));
    }

  // Trace Rx at UdpServer/PacketSink applications
  Ptr<UdpServer> server1 = DynamicCast<UdpServer>(serverApps1.Get(0));
  server1->TraceConnect("Rx", "", MakeBoundCallback(&RxTrace, "/n3/udp-server1"));
  Ptr<UdpServer> server2 = DynamicCast<UdpServer>(serverApps2.Get(0));
  server2->TraceConnect("Rx", "", MakeBoundCallback(&RxTrace, "/n1/udp-server2"));
  Ptr<PacketSink> sinkTcp = DynamicCast<PacketSink>(tcpSinkApp.Get(0));
  sinkTcp->TraceConnect("Rx", "", MakeBoundCallback(&RxTrace, "/n3/tcp-sink"));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  traceFile.close();

  return 0;
}