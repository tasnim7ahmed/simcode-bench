#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/queue-disc-module.h"
#include <fstream>

using namespace ns3;

static std::ofstream traceFile;

void RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  traceFile << Simulator::Now().GetSeconds() << " RX " << context << " UID=" << packet->GetUid() << " " << packet->GetSize() << "B" << std::endl;
}

void QueueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  Ptr<Packet> pkt = item->GetPacket();
  traceFile << Simulator::Now().GetSeconds() << " ENQUEUE " << context << " UID=" << pkt->GetUid() << " " << pkt->GetSize() << "B" << std::endl;
}

void DequeueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  Ptr<Packet> pkt = item->GetPacket();
  traceFile << Simulator::Now().GetSeconds() << " DEQUEUE " << context << " UID=" << pkt->GetUid() << " " << pkt->GetSize() << "B" << std::endl;
}

void DropTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  Ptr<Packet> pkt = item->GetPacket();
  traceFile << Simulator::Now().GetSeconds() << " DROP " << context << " UID=" << pkt->GetUid() << " " << pkt->GetSize() << "B" << std::endl;
}

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  // Open trace file
  traceFile.open("simple-error-model.tr", std::ios::out);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);
  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n2 = nodes.Get(2);
  Ptr<Node> n3 = nodes.Get(3);

  // Links
  PointToPointHelper p2p_5mbps_2ms;
  p2p_5mbps_2ms.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_5mbps_2ms.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p_5mbps_2ms.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  PointToPointHelper p2p_1_5mbps_10ms;
  p2p_1_5mbps_10ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p_1_5mbps_10ms.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p_1_5mbps_10ms.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  // n0 <-> n2
  NetDeviceContainer ndc_n0n2 = p2p_5mbps_2ms.Install(n0, n2);

  // n1 <-> n2
  NetDeviceContainer ndc_n1n2 = p2p_5mbps_2ms.Install(n1, n2);

  // n3 <-> n2
  NetDeviceContainer ndc_n3n2 = p2p_1_5mbps_10ms.Install(n3, n2);

  // Install stacks
  InternetStackHelper stack;
  stack.Install(nodes);

  // Address assignment
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n2 = ipv4.Assign(ndc_n0n2);

  ipv4.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n2 = ipv4.Assign(ndc_n1n2);

  ipv4.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n3n2 = ipv4.Assign(ndc_n3n2);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Error models
  // Rate Error model for n2 device 0 (connected to n0)
  Ptr<RateErrorModel> rateError = CreateObject<RateErrorModel>();
  rateError->SetAttribute("ErrorRate", DoubleValue(0.001));
  ndc_n0n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(rateError));

  // Burst error model for n2 device 1 (connected to n1)
  Ptr<BurstErrorModel> burstError = CreateObject<BurstErrorModel>();
  burstError->SetAttribute("ErrorRate", DoubleValue(0.01));
  ndc_n1n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(burstError));

  // List error model for n2 device 2 (connected to n3)
  Ptr<ListErrorModel> listError = CreateObject<ListErrorModel>();
  std::list<uint32_t> badUids;
  badUids.push_back(11);
  badUids.push_back(17);
  listError->SetList(badUids);
  ndc_n3n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(listError));

  // App 1: CBR/UDP from n0 to n3
  uint16_t udpport1 = 8000;
  UdpServerHelper udpServer1(udpport1);
  ApplicationContainer serverApps1 = udpServer1.Install(n3);
  serverApps1.Start(Seconds(0.0));
  serverApps1.Stop(Seconds(2.0));

  UdpClientHelper udpClient1(if_n3n2.GetAddress(0), udpport1);
  udpClient1.SetAttribute("MaxPackets", UintegerValue(0));
  udpClient1.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  udpClient1.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer clientApps1 = udpClient1.Install(n0);
  clientApps1.Start(Seconds(0.01));
  clientApps1.Stop(Seconds(2.0));

  // App 2: CBR/UDP from n3 to n1
  uint16_t udpport2 = 8001;
  UdpServerHelper udpServer2(udpport2);
  ApplicationContainer serverApps2 = udpServer2.Install(n1);
  serverApps2.Start(Seconds(0.0));
  serverApps2.Stop(Seconds(2.0));

  UdpClientHelper udpClient2(if_n1n2.GetAddress(0), udpport2);
  udpClient2.SetAttribute("MaxPackets", UintegerValue(0));
  udpClient2.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  udpClient2.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer clientApps2 = udpClient2.Install(n3);
  clientApps2.Start(Seconds(0.01));
  clientApps2.Stop(Seconds(2.0));

  // App 3: FTP/TCP from n0 to n3, start at 1.2s, stop at 1.35s
  uint16_t tcpport = 9000;
  Address sinkAddress(InetSocketAddress(if_n3n2.GetAddress(0), tcpport));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer tcpSinkApp = packetSinkHelper.Install(n3);
  tcpSinkApp.Start(Seconds(1.15));
  tcpSinkApp.Stop(Seconds(2.0));

  BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
  bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer bulkSendApp = bulkSender.Install(n0);
  bulkSendApp.Start(Seconds(1.2));
  bulkSendApp.Stop(Seconds(1.35));

  // QUEUE and RX Tracing
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    for (uint32_t j = 0; j < node->GetNDevices(); ++j)
    {
      Ptr<NetDevice> dev = node->GetDevice(j);
      // QueueDisc tracing for PointToPointNetDevice
      Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice>(dev);
      if (p2pDev)
      {
        Ptr<Queue<Packet>> queue = p2pDev->GetQueue();
        if (queue)
        {
          std::string ctx = "Node" + std::to_string(i) + "/Device" + std::to_string(j);
          queue->TraceConnect("Enqueue", ctx, MakeBoundCallback([](std::string context, Ptr<const Packet> packet){
            traceFile << Simulator::Now().GetSeconds() << " ENQUEUE " << context << " UID=" << packet->GetUid() << std::endl;
          }, ctx));
          queue->TraceConnect("Dequeue", ctx, MakeBoundCallback([](std::string context, Ptr<const Packet> packet){
            traceFile << Simulator::Now().GetSeconds() << " DEQUEUE " << context << " UID=" << packet->GetUid() << std::endl;
          }, ctx));
          queue->TraceConnect("Drop", ctx, MakeBoundCallback([](std::string context, Ptr<const Packet> packet){
            traceFile << Simulator::Now().GetSeconds() << " DROP " << context << " UID=" << packet->GetUid() << std::endl;
          }, ctx));
        }
      }
    }
  }
  // Packet reception (sink) tracing
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&RxTrace));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  traceFile.close();
  return 0;
}