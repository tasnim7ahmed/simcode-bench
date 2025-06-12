#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

void
CwndTracer(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueTracer(Ptr<QueueDisc> queue, Ptr<OutputStreamWrapper> stream)
{
  *stream->GetStream() << Simulator::Now().GetSeconds()
                       << "\t" << queue->GetCurrentSize().GetValue() << std::endl;
  Simulator::Schedule(Seconds(0.01), &QueueTracer, queue, stream);
}

void
experiment (const std::string &queueType, const std::string &cwndTraceFile, const std::string &queueTraceFile)
{
  uint32_t nSenders = 7;
  double simTime = 20.0;

  NodeContainer leftNodes, rightNodes, routerNodes;
  leftNodes.Create(nSenders);
  rightNodes.Create(1);
  routerNodes.Create(2);

  InternetStackHelper stack;
  stack.Install(leftNodes);
  stack.Install(rightNodes);
  stack.Install(routerNodes);

  PointToPointHelper p2pLeaf, p2pRouter;
  p2pLeaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2pLeaf.SetChannelAttribute("Delay", StringValue("1ms"));

  p2pRouter.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
  p2pRouter.SetChannelAttribute("Delay", StringValue("10ms"));

  std::vector<NetDeviceContainer> leftDevices(nSenders);

  // Left side links
  for (uint32_t i = 0; i < nSenders; ++i)
  {
    NodeContainer nc(leftNodes.Get(i), routerNodes.Get(0));
    leftDevices[i] = p2pLeaf.Install(nc);
  }

  // Right side link
  NodeContainer rightLink(routerNodes.Get(1), rightNodes.Get(0));
  NetDeviceContainer rightDevices = p2pLeaf.Install(rightLink);

  // Router-to-router link (the bottleneck)
  NodeContainer routerLink(routerNodes.Get(0), routerNodes.Get(1));
  NetDeviceContainer routerRouterDevices = p2pRouter.Install(routerLink);

  // Assign Addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> leftIfs(nSenders);

  for (uint32_t i = 0; i < nSenders; ++i)
  {
    std::ostringstream subnet;
    subnet << "10.1." << i+1 << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    leftIfs[i] = address.Assign(leftDevices[i]);
  }
  address.SetBase("10.1.100.0", "255.255.255.0");
  Ipv4InterfaceContainer rightIfs = address.Assign(rightDevices);

  address.SetBase("10.1.200.0", "255.255.255.0");
  Ipv4InterfaceContainer routerIfs = address.Assign(routerRouterDevices);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Traffic Control: queue disc on the bottleneck
  TrafficControlHelper tch;
  if (queueType == "cobalt")
  {
    tch.SetRootQueueDisc("ns3::CobaltQueueDisc");
  }
  else if (queueType == "codel")
  {
    tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
  }
  else
  {
    NS_ABORT_MSG("Unknown queue disc");
  }
  QueueDiscContainer qdiscs = tch.Install(routerRouterDevices.Get(0));

  // TCP sender: nSenders-1 senders
  uint16_t tcpPort = 50000;
  for (uint32_t i = 0; i < nSenders-1; ++i)
  {
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort + i));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (rightNodes.Get(0));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (simTime));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(leftNodes.Get(i), TcpSocketFactory::GetTypeId ());
    InetSocketAddress remoteAddr (rightIfs.GetAddress(1), tcpPort + i);
    remoteAddr.SetTos (0xb8); // AF41
    OnOffHelper client ("ns3::TcpSocketFactory", Address (remoteAddr));
    client.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
    client.SetAttribute ("PacketSize", UintegerValue (1448));
    client.SetAttribute ("StartTime", TimeValue (Seconds (1.0 + i * 0.1)));
    client.SetAttribute ("StopTime", TimeValue (Seconds (simTime-1)));
    client.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
    ApplicationContainer app = client.Install (leftNodes.Get(i));

    // Tracing congestion window for first TCP flow
    if (i == 0) 
    {
      AsciiTraceHelper ascii;
      Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(cwndTraceFile);
      ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndTracer, stream));
    }
  }

  // UDP sender: the last sender
  uint16_t udpPort = 60000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), udpPort));
  PacketSinkHelper udpSinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer udpSinkApp = udpSinkHelper.Install (rightNodes.Get(0));
  udpSinkApp.Start (Seconds (0.0));
  udpSinkApp.Stop (Seconds (simTime));

  OnOffHelper udpClient ("ns3::UdpSocketFactory",InetSocketAddress (rightIfs.GetAddress(1), udpPort));
  udpClient.SetAttribute ("DataRate", DataRateValue (DataRate ("3Mbps")));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1400));
  udpClient.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  udpClient.SetAttribute ("StopTime", TimeValue (Seconds (simTime-1)));
  ApplicationContainer udpApp = udpClient.Install (leftNodes.Get(nSenders-1));

  // QueueDisc trace
  AsciiTraceHelper asciiQ;
  Ptr<OutputStreamWrapper> qsStream = asciiQ.CreateFileStream(queueTraceFile);
  Simulator::ScheduleNow(&QueueTracer, qdiscs.Get(0), qsStream);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

int main(int argc, char *argv[])
{
  experiment("cobalt", "tcp-cwnd-cobalt.txt", "queue-cobalt.txt");
  experiment("codel", "tcp-cwnd-codel.txt", "queue-codel.txt");
  return 0;
}