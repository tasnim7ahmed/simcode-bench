#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStarSimulation");

void
QueueTrace (Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> packet)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << context << " " << packet->GetUid () << std::endl;
}

void
RxTrace (Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> packet, const Address &addr)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << context << " " << packet->GetUid () << std::endl;
}

int
main (int argc, char *argv[])
{
  uint32_t numNodes = 9;
  std::string dataRate = "5Mbps";
  uint32_t pktSize = 1024;
  double simTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of total nodes in star (>= 2)", numNodes);
  cmd.AddValue ("dataRate", "Data rate for point-to-point links and traffic", dataRate);
  cmd.AddValue ("packetSize", "Packet size in bytes", pktSize);
  cmd.Parse (argc, argv);

  if (numNodes < 2) numNodes = 2;

  uint32_t numArms = numNodes - 1;

  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer armNodes;
  armNodes.Create (numArms);

  NodeContainer allNodes;
  allNodes.Add (hubNode);
  allNodes.Add (armNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (1000));

  std::vector<NetDeviceContainer> deviceArms;
  std::vector<NodeContainer> linkNodes;

  for (uint32_t i = 0; i < numArms; ++i)
    {
      NodeContainer nc = NodeContainer (hubNode.Get (0), armNodes.Get (i));
      NetDeviceContainer ndc = p2p.Install (nc);
      deviceArms.push_back (ndc);
      linkNodes.push_back (nc);
    }

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> ifaceArms;
  char net[20];

  for (uint32_t i = 0; i < numArms; ++i)
    {
      snprintf (net, sizeof (net), "10.1.%u.0", i+1);
      address.SetBase (net, "255.255.255.0");
      ifaceArms.push_back (address.Assign (deviceArms[i]));
    }

  // Install PacketSink on hub node for each connection
  std::vector<uint16_t> portVec;
  std::vector<ApplicationContainer> sinkApps;

  for (uint32_t i = 0; i < numArms; ++i)
    {
      uint16_t port = 9000 + i;
      Address hubLocalAddress (InetSocketAddress (ifaceArms[i].GetAddress (0), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", hubLocalAddress);
      ApplicationContainer app = sinkHelper.Install (hubNode.Get(0));
      app.Start (Seconds (0.0));
      app.Stop (Seconds (simTime));
      sinkApps.push_back (app);
      portVec.push_back (port);
    }

  // Install OnOffApplication on each arm, sending to hub node
  for (uint32_t i = 0; i < numArms; ++i)
    {
      OnOffHelper onoff ("ns3::TcpSocketFactory", Address());
      AddressValue remoteAddress (InetSocketAddress (ifaceArms[i].GetAddress (0), portVec[i]));
      onoff.SetAttribute ("Remote", remoteAddress);
      onoff.SetAttribute ("DataRate", StringValue (dataRate));
      onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime-0.01)));
      onoff.Install (armNodes.Get (i));
    }

  // Enable pcap on all hub interfaces with correct filenames
  for (uint32_t i = 0; i < numArms; ++i)
    {
      std::ostringstream oss;
      oss << "tcp-star-server-" << 0 << "-" << i << ".pcap";
      p2p.EnablePcap (oss.str (), deviceArms[i].Get (0), true, true);
    }

  // Setup tracing
  AsciiTraceHelper asciiTrace;
  Ptr<OutputStreamWrapper> stream = asciiTrace.CreateFileStream ("tcp-star-server.tr");

  // Attach queue enqueue tracing on each hub device interface
  for (uint32_t i = 0; i < numArms; ++i)
    {
      Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice> (deviceArms[i].Get (0));
      if (dev)
        {
          Ptr<Queue<Packet>> queue = dev->GetQueue ();
          if (queue)
            {
              std::ostringstream ctx;
              ctx << "/NodeList/0/DeviceList/" << dev->GetIfIndex () << "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
              Config::ConnectWithoutContext (ctx.str(), MakeBoundCallback (&QueueTrace, stream, ctx.str()));
            }
        }
    }

  // Attach Rx trace sinks at hub node
  for (uint32_t i = 0; i < numArms; ++i)
    {
      Ptr<PacketSink> sink = StaticCast<PacketSink> (sinkApps[i].Get (0));
      std::ostringstream ctx;
      ctx << "/NodeList/0/ApplicationList/" << sinkApps[i].Get (0)->GetId () << "/$ns3::PacketSink/Rx";
      Config::ConnectWithoutContext (ctx.str (), MakeBoundCallback (&RxTrace, stream, ctx.str ()));
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}