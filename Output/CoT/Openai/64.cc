#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/pcap-file-wrapper.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStarServer");

void
QueueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  static std::ofstream out("tcp-star-server.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " " << context << " Queue: Packet enqueued\n";
}

void
PacketReceptionTrace (Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out("tcp-star-server.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " Packet received at server\n";
}

int
main (int argc, char *argv[])
{
  uint32_t numNodes = 9;
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 1024;

  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of total nodes (>=2), first is hub", numNodes);
  cmd.AddValue ("dataRate", "Data rate for CBR traffic", dataRate);
  cmd.AddValue ("packetSize", "Packet size for CBR traffic (bytes)", packetSize);
  cmd.Parse (argc, argv);

  if (numNodes < 2)
    {
      NS_LOG_ERROR ("Star topology requires at least 2 nodes.");
      return 1;
    }

  NodeContainer nodes;
  nodes.Create (numNodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  std::vector<NodeContainer> arms;
  std::vector<NetDeviceContainer> devices;
  std::vector<Ipv4InterfaceContainer> interfaces;
  Ipv4AddressHelper address;
  char baseIpStr[16];
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      NodeContainer arm;
      arm.Add (nodes.Get (0)); // hub
      arm.Add (nodes.Get (i)); // arm node
      arms.push_back (arm);

      PointToPointHelper p2p;
      p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
      p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

      NetDeviceContainer dev = p2p.Install (arm);
      devices.push_back (dev);

      snprintf(baseIpStr, sizeof(baseIpStr), "10.1.%d.0", i);
      address.SetBase (baseIpStr, "255.255.255.0");
      Ipv4InterfaceContainer iface = address.Assign (dev);
      interfaces.push_back (iface);

      // Enable pcap tracing for both interfaces of this arm link
      for (uint32_t j = 0; j < 2; ++j)
        {
          std::ostringstream oss;
          oss << "tcp-star-server-" << (arm.Get(j))->GetId () << "-" << j << ".pcap";
          p2p.EnablePcap (oss.str (), dev.Get(j), true, true);
        }
    }

  uint16_t sinkPort = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (20.0));
  Ptr<Application> app = sinkApp.Get (0);

  // Connect trace for packet reception
  app->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketReceptionTrace));

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetSize));

  ApplicationContainer cbrApps;
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      OnOffHelper clientHelper ("ns3::TcpSocketFactory",
        Address (InetSocketAddress (interfaces[i-1].GetAddress (0), sinkPort)));
      clientHelper.SetAttribute ("DataRate", StringValue (dataRate));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
      clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
      clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0 + 0.2 * i)));
      clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (19.0)));
      ApplicationContainer app = clientHelper.Install (nodes.Get (i));
      cbrApps.Add (app);
    }

  // Connect queue trace on each point-to-point net device at hub
  for (uint32_t i = 0; i < devices.size(); ++i)
    {
      Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice> (devices[i].Get(0));
      Ptr<Queue<Packet> > queue = dev->GetQueue ();
      if (queue)
        {
          queue->TraceConnect ("Enqueue", std::to_string(i), MakeCallback (&QueueTrace));
        }
      // For traffic control layer (for modern NS-3)
      Ptr<NetDeviceQueueInterface> ndq = dev->GetObject<NetDeviceQueueInterface> ();
      if (ndq && ndq->GetNQueueDisc () > 0)
        {
          Ptr<QueueDisc> qdisc = ndq->GetQueueDisc (0);
          if (qdisc)
            {
              qdisc->TraceConnect ("Enqueue", std::to_string(i), MakeCallback (&QueueTrace));
            }
        }
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}