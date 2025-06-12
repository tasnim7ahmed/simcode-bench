#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/queue-size.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedPtpcCsmaCbrUdpExample");

static void
PacketReceptionTrace(std::string context, Ptr<const Packet> packet, const Address& address)
{
  static std::ofstream rxTraceFile;
  static bool initialized = false;
  if (!initialized)
    {
      rxTraceFile.open("packet_receptions.txt", std::ios_base::out);
      initialized = true;
    }
  rxTraceFile << Simulator::Now().GetSeconds() << "s "
              << context << " Received packet of "
              << packet->GetSize() << " bytes" << std::endl;
}

static void
QueueTrace (std::string context, uint32_t oldVal, uint32_t newVal)
{
  static std::ofstream queueTraceFile;
  static bool initialized = false;
  if (!initialized)
    {
      queueTraceFile.open("queue_trace.txt", std::ios_base::out);
      initialized = true;
    }
  queueTraceFile << Simulator::Now().GetSeconds() << "s "
                 << context << " Queue length change: "
                 << oldVal << " -> " << newVal << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer n0n2;
  n0n2.Add(CreateObject<Node>());
  n0n2.Add(CreateObject<Node>());
  NodeContainer n1n2;
  n1n2.Add(CreateObject<Node>());
  n1n2.Add(n0n2.Get(1)); // n2 is shared

  NodeContainer csmaNodes;
  csmaNodes.Add(n0n2.Get(1)); // n2
  csmaNodes.Add(CreateObject<Node>()); // n3
  csmaNodes.Add(CreateObject<Node>()); // n4
  csmaNodes.Add(CreateObject<Node>()); // n5

  NodeContainer n5n6;
  n5n6.Add(csmaNodes.Get(3)); // n5
  n5n6.Add(CreateObject<Node>()); // n6

  Ptr<Node> n0 = n0n2.Get(0);
  Ptr<Node> n1 = n1n2.Get(0);
  Ptr<Node> n2 = n0n2.Get(1);
  Ptr<Node> n3 = csmaNodes.Get(1);
  Ptr<Node> n4 = csmaNodes.Get(2);
  Ptr<Node> n5 = csmaNodes.Get(3);
  Ptr<Node> n6 = n5n6.Get(1);

  // Point-to-point helpers
  PointToPointHelper p2p_n0n2;
  p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2p_n1n2;
  p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2p_n5n6;
  p2p_n5n6.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n5n6.SetChannelAttribute("Delay", StringValue("2ms"));

  // CSMA helper
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  // Install point-to-point devices
  NetDeviceContainer dev_n0n2 = p2p_n0n2.Install(n0, n2);
  NetDeviceContainer dev_n1n2 = p2p_n1n2.Install(n1, n2);
  NetDeviceContainer dev_n5n6 = p2p_n5n6.Install(n5, n6);

  // Install csma devices
  NetDeviceContainer dev_csma = csma.Install(csmaNodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n2 = ipv4.Assign(dev_n0n2);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n2 = ipv4.Assign(dev_n1n2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_csma = ipv4.Assign(dev_csma);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n5n6 = ipv4.Assign(dev_n5n6);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Setup OnOff CBR/UDP application (n0 -> n6)
  uint16_t port = 9999;
  Address sinkAddress (InetSocketAddress (if_n5n6.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(n6);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(20.0));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", StringValue("300bps"));
  onoff.SetAttribute ("PacketSize", UintegerValue(50));
  onoff.SetAttribute ("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute ("StopTime", TimeValue(Seconds(19.0)));
  ApplicationContainer app = onoff.Install(n0);

  // Enable pcap tracing for all point-to-point and csma channels
  p2p_n0n2.EnablePcapAll("n0n2");
  p2p_n1n2.EnablePcapAll("n1n2");
  p2p_n5n6.EnablePcapAll("n5n6");
  csma.EnablePcapAll("csma");

  // Trace network queues on each netdevice's tx queue
  for (uint32_t i = 0; i < dev_n0n2.GetN(); ++i)
    {
      Ptr<NetDevice> nd = dev_n0n2.Get(i);
      Ptr<PointToPointNetDevice> ptp = DynamicCast<PointToPointNetDevice>(nd);
      if (ptp)
        {
          Ptr<Queue<Packet>> q = ptp->GetQueue();
          if (!q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueTrace)))
            {
              q->TraceConnect("PacketsInQueue", std::string("dev_n0n2"), MakeCallback(&QueueTrace));
            }
        }
    }
  for (uint32_t i = 0; i < dev_n1n2.GetN(); ++i)
    {
      Ptr<NetDevice> nd = dev_n1n2.Get(i);
      Ptr<PointToPointNetDevice> ptp = DynamicCast<PointToPointNetDevice>(nd);
      if (ptp)
        {
          Ptr<Queue<Packet>> q = ptp->GetQueue();
          if (!q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueTrace)))
            {
              q->TraceConnect("PacketsInQueue", std::string("dev_n1n2"), MakeCallback(&QueueTrace));
            }
        }
    }
  for (uint32_t i = 0; i < dev_n5n6.GetN(); ++i)
    {
      Ptr<NetDevice> nd = dev_n5n6.Get(i);
      Ptr<PointToPointNetDevice> ptp = DynamicCast<PointToPointNetDevice>(nd);
      if (ptp)
        {
          Ptr<Queue<Packet>> q = ptp->GetQueue();
          if (!q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueTrace)))
            {
              q->TraceConnect("PacketsInQueue", std::string("dev_n5n6"), MakeCallback(&QueueTrace));
            }
        }
    }
  for (uint32_t i = 0; i < dev_csma.GetN(); ++i)
    {
      Ptr<NetDevice> nd = dev_csma.Get(i);
      Ptr<CsmaNetDevice> csmaNd = DynamicCast<CsmaNetDevice>(nd);
      if (csmaNd)
        {
          Ptr<Queue<Packet>> q = csmaNd->GetQueue();
          if (!q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueTrace)))
            {
              q->TraceConnect("PacketsInQueue", std::string("csma"), MakeCallback(&QueueTrace));
            }
        }
    }

  // Trace packet receptions at sink
  Ptr<Application> sinkApp = sinkApps.Get(0);
  Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(sinkApp);
  pktSink->TraceConnect("Rx", std::string("SinkRx"), MakeCallback(&PacketReceptionTrace));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}