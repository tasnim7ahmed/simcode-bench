#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/trace-helper.h"
#include <fstream>

using namespace ns3;

void
PacketReceptionCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream outfile("packet-receptions.txt", std::ios_base::app);
  outfile << Simulator::Now().GetSeconds() << "s " << context << " Received " << packet->GetSize() << " bytes\n";
}

void
QueueTraceCallback(std::string context, Ptr<const QueueDiscItem> item)
{
  static std::ofstream outfile("queue-trace.txt", std::ios_base::app);
  outfile << Simulator::Now().GetSeconds() << "s " << context << " " << item->GetPacket()->GetSize() << " bytes enqueued\n";
}

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer n0n2, n1n2, csmaNodes, n5n6;
  n0n2.Add(CreateObject<Node>());
  n0n2.Add(CreateObject<Node>());
  n1n2.Add(CreateObject<Node>());
  n1n2.Add(CreateObject<Node>());
  for (uint32_t i = 0; i < 4; ++i)
    csmaNodes.Add(CreateObject<Node>());
  n5n6.Add(csmaNodes.Get(3)); // n5
  n5n6.Add(CreateObject<Node>());

  Ptr<Node> n0 = n0n2.Get(0);
  Ptr<Node> n1 = n1n2.Get(0);
  Ptr<Node> n2 = n0n2.Get(1);
  Ptr<Node> n3 = csmaNodes.Get(1);
  Ptr<Node> n4 = csmaNodes.Get(2);
  Ptr<Node> n5 = csmaNodes.Get(3);
  Ptr<Node> n6 = n5n6.Get(1);

  // To map: nodes  n0:0  n1:1  n2:2  n3:3  n4:4  n5:5  n6:6
  // But csmaNodes is: [n2, n3, n4, n5]
  // Assigning:
  // n2 = csmaNodes.Get(0)
  // n3 = csmaNodes.Get(1)
  // n4 = csmaNodes.Get(2)
  // n5 = csmaNodes.Get(3)

  // Connect n0n2, n1n2, n5n6: point-to-point
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d0n2 = p2p.Install(n0n2); // n0 <-> n2
  NetDeviceContainer d1n2 = p2p.Install(n1n2); // n1 <-> n2
  NetDeviceContainer d5n6 = p2p.Install(n5n6); // n5 <-> n6

  // CSMA: n2, n3, n4, n5
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

  // Stack
  InternetStackHelper stack;
  stack.Install(n0);
  stack.Install(n1);
  stack.Install(csmaNodes);
  stack.Install(n6);

  // Assign IPs
  Ipv4AddressHelper address;
  // n0n2
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0n2 = address.Assign(d0n2);

  // n1n2
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1n2 = address.Assign(d1n2);

  // csma
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer icsma = address.Assign(csmaDevices);

  // n5n6
  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i5n6 = address.Assign(d5n6);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Applications
  uint16_t sinkPort = 9000;
  Address sinkAddress(InetSocketAddress(i5n6.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(n6);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(20.0));

  OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("300bps")));
  onoff.SetAttribute("PacketSize", UintegerValue(50));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApps = onoff.Install(n0);
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(20.0));

  // Trace Packet Receptions at n6
  Config::Connect("/NodeList/" + std::to_string(n6->GetId()) + "/ApplicationList/0/$ns3::PacketSink/Rx",
                  MakeCallback(&PacketReceptionCallback));

  // Trace Network Queues
  for (uint32_t i = 0; i < d0n2.GetN(); ++i)
  {
    std::string path = d0n2.Get(i)->GetInstanceTypeId().GetName();
    Ptr<NetDevice> nd = d0n2.Get(i);
    Ptr<PointToPointNetDevice> p2pnd = DynamicCast<PointToPointNetDevice>(nd);
    if (p2pnd)
    {
      Ptr<Queue<Packet>> queue = p2pnd->GetQueue();
      if (queue)
      {
        queue->TraceConnect("Enqueue", std::to_string(nd->GetNode()->GetId()) + "-d0n2-enqueue", MakeBoundCallback(&QueueTraceCallback));
      }
    }
  }
  for (uint32_t i = 0; i < d1n2.GetN(); ++i)
  {
    std::string path = d1n2.Get(i)->GetInstanceTypeId().GetName();
    Ptr<NetDevice> nd = d1n2.Get(i);
    Ptr<PointToPointNetDevice> p2pnd = DynamicCast<PointToPointNetDevice>(nd);
    if (p2pnd)
    {
      Ptr<Queue<Packet>> queue = p2pnd->GetQueue();
      if (queue)
      {
        queue->TraceConnect("Enqueue", std::to_string(nd->GetNode()->GetId()) + "-d1n2-enqueue", MakeBoundCallback(&QueueTraceCallback));
      }
    }
  }
  for (uint32_t i = 0; i < d5n6.GetN(); ++i)
  {
    std::string path = d5n6.Get(i)->GetInstanceTypeId().GetName();
    Ptr<NetDevice> nd = d5n6.Get(i);
    Ptr<PointToPointNetDevice> p2pnd = DynamicCast<PointToPointNetDevice>(nd);
    if (p2pnd)
    {
      Ptr<Queue<Packet>> queue = p2pnd->GetQueue();
      if (queue)
      {
        queue->TraceConnect("Enqueue", std::to_string(nd->GetNode()->GetId()) + "-d5n6-enqueue", MakeBoundCallback(&QueueTraceCallback));
      }
    }
  }
  for (uint32_t i = 0; i < csmaDevices.GetN(); ++i)
  {
    Ptr<NetDevice> nd = csmaDevices.Get(i);
    Ptr<CsmaNetDevice> csmand = DynamicCast<CsmaNetDevice>(nd);
    if (csmand)
    {
      Ptr<Queue<Packet>> queue = csmand->GetQueue();
      if (queue)
      {
        queue->TraceConnect("Enqueue", std::to_string(nd->GetNode()->GetId()) + "-csma-enqueue", MakeBoundCallback(&QueueTraceCallback));
      }
    }
  }

  // Enable pcap tracing for all channels
  p2p.EnablePcapAll("mixed-topology-p2p");
  csma.EnablePcapAll("mixed-topology-csma", false);

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}