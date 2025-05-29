#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellTcpUdpQueueDiscSim");

void
CwndTracer (std::string filename, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::map<std::string, Ptr<OutputStreamWrapper>> c_writers;
  if (c_writers.find(filename) == c_writers.end())
    {
      AsciiTraceHelper ascii;
      c_writers[filename] = ascii.CreateFileStream(filename);
      *(c_writers[filename]->GetStream()) << "Time(s)\tCwnd(Bytes)" << std::endl;
    }
  *(c_writers[filename]->GetStream()) << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueSizeTracer (std::string filename, uint32_t oldVal, uint32_t newVal)
{
  static std::map<std::string, Ptr<OutputStreamWrapper>> q_writers;
  if (q_writers.find(filename) == q_writers.end())
    {
      AsciiTraceHelper ascii;
      q_writers[filename] = ascii.CreateFileStream(filename);
      *(q_writers[filename]->GetStream()) << "Time(s)\tQueueSize(Pkts)" << std::endl;
    }
  *(q_writers[filename]->GetStream()) << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void experiment (std::string queueDiscType, std::string cwndTraceFilename, std::string queueTraceFilename, double simDuration)
{
  uint32_t nSenders = 7;
  uint32_t nReceivers = 1;
  uint32_t nFlows = nSenders;
  // Nodes: senders, left router, right router, receiver(s)
  NodeContainer senders;
  senders.Create(nSenders);
  NodeContainer leftRouter;
  leftRouter.Create(1);
  NodeContainer rightRouter;
  rightRouter.Create(1);
  NodeContainer receiver;
  receiver.Create(nReceivers);

  // Connections: senders <-> leftRouter, rightRouter <-> receiver, leftRouter <-> rightRouter (bottleneck)
  NetDeviceContainer senderDevices, leftRouterDevices1, rightRouterDevices1, receiverDevices;
  std::vector<NetDeviceContainer> accessDevContainers;
  InternetStackHelper internet;
  internet.Install(senders);
  internet.Install(leftRouter);
  internet.Install(rightRouter);
  internet.Install(receiver);

  // Point-to-point helper for access links
  PointToPointHelper access;
  access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  access.SetChannelAttribute("Delay", StringValue("2ms"));

  // sender[i] <-> left router
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      NodeContainer pair;
      pair.Add(senders.Get(i));
      pair.Add(leftRouter.Get(0));
      NetDeviceContainer ndc = access.Install(pair);
      accessDevContainers.push_back(ndc);
    }

  // rightRouter <-> receiver
  NetDeviceContainer right_recv = access.Install(NodeContainer(rightRouter.Get(0), receiver.Get(0)));

  // Point-to-point helper for bottleneck
  PointToPointHelper bottleneck;
  bottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  bottleneck.SetChannelAttribute("Delay", StringValue("20ms"));

  // leftRouter <-> rightRouter
  NetDeviceContainer core = bottleneck.Install(NodeContainer(leftRouter.Get(0), rightRouter.Get(0)));

  // Traffic control on bottleneck (leftRouter side)
  TrafficControlHelper tch;
  uint16_t handle;
  if (queueDiscType == "cobalt")
    {
      handle = tch.SetRootQueueDisc ("ns3::CobaltQueueDisc");
    }
  else if (queueDiscType == "codel")
    {
      handle = tch.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    }
  else
    {
      NS_ABORT_MSG ("Unsupported queue disc type");
    }
  tch.Install(core.Get(0));

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> senderInterfaces;

  // Senders to left router subnets: 10.1.i.0
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      ipv4.SetBase (Ipv4Address(subnet.str().c_str()), "255.255.255.0");
      senderInterfaces.push_back(ipv4.Assign(accessDevContainers[i]));
    }

  // Bottleneck subnet: 10.2.1.0
  ipv4.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = ipv4.Assign(core);

  // Right router - receiver subnet: 10.3.1.0
  ipv4.SetBase ("10.3.1.0", "255.255.255.0");
  Ipv4InterfaceContainer recvInterfaces = ipv4.Assign(right_recv);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Set up applications (first sender is TCP, rest are UDP)
  uint16_t tcpPort = 5000;
  uint16_t udpPort = 6000;

  // TCP sink on receiver
  Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", localAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (receiver.Get(0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simDuration));

  // Create TCP source (BulkSend)
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (senders.Get(0), TcpSocketFactory::GetTypeId ());
  BulkSendHelper bulkSend ("ns3::TcpSocketFactory",
                           InetSocketAddress (recvInterfaces.GetAddress (0), tcpPort));
  bulkSend.SetAttribute ("MaxBytes", UintegerValue (0));
  bulkSend.SetAttribute ("SendSize", UintegerValue (1448));
  ApplicationContainer sourceApp = bulkSend.Install (senders.Get(0));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (simDuration));

  // UDP sinks and sources
  // Install one UDP flow per remaining sender
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nSenders; ++i)
    {
      // On receiver: UDP sink
      PacketSinkHelper udpSink ("ns3::UdpSocketFactory",
                                InetSocketAddress (Ipv4Address::GetAny(), udpPort + i));
      serverApps.Add(udpSink.Install (receiver.Get (0)));
      // On sender: UDP OnOff app
      OnOffHelper udpClient ("ns3::UdpSocketFactory",
                             InetSocketAddress (recvInterfaces.GetAddress (0), udpPort + i));
      udpClient.SetAttribute("DataRate", StringValue("3Mbps"));
      udpClient.SetAttribute("PacketSize", UintegerValue(1400));
      udpClient.SetAttribute("StartTime", TimeValue(Seconds (1.5 + i*0.1)));
      udpClient.SetAttribute("StopTime", TimeValue(Seconds (simDuration)));
      clientApps.Add(udpClient.Install (senders.Get(i)));
    }

  // TCP congestion window trace (only for the BulkSend sender)
  std::ostringstream cwndStreamName;
  cwndStreamName << cwndTraceFilename;
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback(&CwndTracer, cwndStreamName.str()));

  // Queue size trace
  std::ostringstream queueStreamName;
  queueStreamName << queueTraceFilename;
  Ptr<PointToPointNetDevice> bottleneckDev = DynamicCast<PointToPointNetDevice>(core.Get(0));
  Ptr<TrafficControlLayer> tcLayer = bottleneckDev->GetNode()->GetObject<TrafficControlLayer>();
  Ptr<QueueDisc> queueDisc = tcLayer->GetRootQueueDiscOnDevice(core.Get(0));
  queueDisc->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback(&QueueSizeTracer, queueStreamName.str()));

  Simulator::Stop (Seconds(simDuration));
  Simulator::Run ();
  Simulator::Destroy ();
}

int main (int argc, char *argv[])
{
  double simDuration = 30.0;

  // First run: COBALT
  experiment ("cobalt", "tcp_cwnd_cobalt.txt", "queue_cobalt.txt", simDuration);

  // Second run: CoDel
  experiment ("codel", "tcp_cwnd_codel.txt", "queue_codel.txt", simDuration);

  return 0;
}