#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTcpSimulation");

static std::map<uint32_t, Time> sentTime;
static std::map<uint32_t, Time> recvTime;
static uint32_t receivedPackets = 0;
static uint32_t lostPackets = 0;
static uint64_t totalBytes = 0;

void
TxTrace (Ptr<const Packet> packet)
{
  uint32_t seq = packet->GetUid ();
  sentTime[seq] = Simulator::Now ();
}

void
RxTraceAddress (std::string context, Ptr<const Packet> packet, const Address &address)
{
  uint32_t seq = packet->GetUid ();
  recvTime[seq] = Simulator::Now ();
  ++receivedPackets;
  totalBytes += packet->GetSize ();
}

void
RxDrop (Ptr<const Packet> p)
{
  ++lostPackets;
}

int
main (int argc, char *argv[])
{
  uint32_t nClients = 5;
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 50;
  double interPacketInterval = 0.01; // seconds

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes: star with nClients clients and 1 server at center
  NodeContainer serverNode;
  serverNode.Create (1);
  NodeContainer clientNodes;
  clientNodes.Create (nClients);
  NodeContainer allNodes (serverNode, clientNodes);

  // Point-to-Point helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices and channels
  std::vector<NetDeviceContainer> deviceContainers;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      NodeContainer link (serverNode.Get (0), clientNodes.Get (i));
      NetDeviceContainer devices = p2p.Install (link);
      deviceContainers.push_back (devices);
    }

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> ifaceContainers;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      Ipv4InterfaceContainer iface = address.Assign (deviceContainers[i]);
      ifaceContainers.push_back (iface);
    }

  // Set up TCP sink at server
  uint16_t port = 5000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer serverApps = packetSinkHelper.Install (serverNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Set up OnOff TCP clients at each client node
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      AddressValue remoteAddress (InetSocketAddress (ifaceContainers[i].GetAddress (0), port));
      OnOffHelper onoff ("ns3::TcpSocketFactory", Address ());
      onoff.SetAttribute ("Remote", remoteAddress);
      onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
      onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
      onoff.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
      onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer clientApp = onoff.Install (clientNodes.Get (i));
      clientApp.Start (Seconds (1.0 + i * 0.05));
      clientApp.Stop (Seconds (9.0));
      clientApps.Add (clientApp);
    }

  // NetAnim setup
  AnimationInterface anim ("star_tcp_netanim.xml");
  anim.SetConstantPosition (serverNode.Get (0), 50, 50);
  double theta = 2 * M_PI / nClients;
  double r = 30.0;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      double x = 50 + r * std::cos (i * theta);
      double y = 50 + r * std::sin (i * theta);
      anim.SetConstantPosition (clientNodes.Get (i), x, y);
    }

  // Enable pcap and ascii trace
  p2p.EnablePcapAll ("star-tcp");

  // Connect TX/RX trace sources for throughput and latency
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<NetDevice> dev = clientNodes.Get (i)->GetDevice (0);
      dev->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxTrace));
    }
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApps.Get (0));
  sink->TraceConnect ("RxWithAddresses", "", MakeCallback (&RxTraceAddress));

  // Packet drop tracing
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice> (deviceContainers[i].Get (1));
      dev->GetQueue ()->TraceConnectWithoutContext ("Drop", MakeCallback (&RxDrop));
    }

  // Run simulation
  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();

  // Calculate throughput [Mbps], latency [ms], and packet loss
  double duration = 8.0; // traffic duration (approx between 1s-9s)
  double throughput = (totalBytes * 8) / (duration * 1e6);
  double avgLatency = 0.0;
  uint32_t count = 0;

  for (auto const& pair : sentTime)
    {
      uint32_t seq = pair.first;
      if (recvTime.count (seq) > 0)
        {
          avgLatency += (recvTime[seq] - sentTime[seq]).GetMilliSeconds ();
          ++count;
        }
    }
  if (count > 0)
    {
      avgLatency /= count;
    }

  std::cout << "---- Simulation Results ----" << std::endl;
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
  std::cout << "Average Latency: " << avgLatency << " ms" << std::endl;
  std::cout << "Packets Received: " << receivedPackets << std::endl;
  std::cout << "Packets Lost: " << lostPackets << std::endl;

  Simulator::Destroy ();
  return 0;
}