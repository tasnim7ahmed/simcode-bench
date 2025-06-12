#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

struct PacketLogEntry
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t packetSize;
  double txTime;
  double rxTime;
};

class StarPacketTracer
{
public:
  StarPacketTracer ();
  void LogTx (Ptr<const Packet> packet, const Address &address);
  void LogRx (Ptr<const Packet> packet, const Address &address);
  void SetSrcDest (uint32_t src, uint32_t dst);
  void WriteCsv (const std::string &filename);

private:
  std::map<uint64_t, std::pair<double, PacketLogEntry>> m_packetMap;
  std::vector<PacketLogEntry> m_entries;
  uint32_t m_srcNode;
  uint32_t m_dstNode;
};

StarPacketTracer::StarPacketTracer () : m_srcNode (0), m_dstNode (0) {}

void
StarPacketTracer::SetSrcDest (uint32_t src, uint32_t dst)
{
  m_srcNode = src;
  m_dstNode = dst;
}

void
StarPacketTracer::LogTx (Ptr<const Packet> packet, const Address &address)
{
  uint64_t uid = packet->GetUid ();
  PacketLogEntry entry;
  entry.srcNode = m_srcNode;
  entry.dstNode = m_dstNode;
  entry.packetSize = packet->GetSize ();
  entry.txTime = Simulator::Now ().GetSeconds ();
  entry.rxTime = 0.0;
  m_packetMap[uid] = std::make_pair (Simulator::Now ().GetSeconds (), entry);
}

void
StarPacketTracer::LogRx (Ptr<const Packet> packet, const Address &address)
{
  uint64_t uid = packet->GetUid ();
  auto it = m_packetMap.find (uid);
  if (it != m_packetMap.end ())
    {
      PacketLogEntry entry = it->second.second;
      entry.rxTime = Simulator::Now ().GetSeconds ();
      m_entries.push_back (entry);
      m_packetMap.erase (it);
    }
}

void
StarPacketTracer::WriteCsv (const std::string &filename)
{
  std::ofstream out (filename, std::ofstream::trunc);
  out << "source,destination,packet_size_bytes,tx_time_sec,rx_time_sec\n";
  for (const auto &entry : m_entries)
    {
      out << entry.srcNode << ',' << entry.dstNode << ',' << entry.packetSize
          << ',' << std::fixed << std::setprecision(9) << entry.txTime
          << ',' << std::fixed << std::setprecision(9) << entry.rxTime << '\n';
    }
  out.close ();
}

int
main (int argc, char *argv[])
{
  uint32_t nLeaf = 4;
  std::string dataRate = "10Mbps";
  std::string delay = "2ms";
  uint32_t packetSize = 1024;
  double simulationTime = 10.0;
  std::string csvFile = "star_tcp_packets.csv";

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer centralNode;
  centralNode.Create (1);
  NodeContainer leafNodes;
  leafNodes.Create (nLeaf);

  NodeContainer allNodes;
  allNodes.Add (centralNode.Get (0));
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      allNodes.Add (leafNodes.Get (i));
    }

  std::vector<NodeContainer> starLinks;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      NodeContainer nc;
      nc.Add (centralNode.Get (0));
      nc.Add (leafNodes.Get (i));
      starLinks.push_back (nc);
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));

  std::vector<NetDeviceContainer> deviceContainers;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      deviceContainers.push_back (p2p.Install (starLinks[i]));
    }

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      interfaces.push_back (address.Assign (deviceContainers[i]));
    }

  uint16_t port = 50000;
  std::vector<ApplicationContainer> sinkApps (nLeaf);
  std::vector<Ptr<StarPacketTracer>> tracers (nLeaf);

  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      Address sinkAddress (InetSocketAddress (interfaces[i].GetAddress (1), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
      sinkApps[i] = sinkHelper.Install (leafNodes.Get (i));
      sinkApps[i].Start (Seconds (0.0));
      sinkApps[i].Stop (Seconds (simulationTime + 1));

      tracers[i] = CreateObject<StarPacketTracer> ();
      tracers[i]->SetSrcDest (0, i + 1);
    }

  ApplicationContainer sourceApps;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      Ptr<Socket> srcSocket = Socket::CreateSocket (centralNode.Get (0), TcpSocketFactory::GetTypeId ());
      Address sinkAddress (InetSocketAddress (interfaces[i].GetAddress (1), port));

      Ptr<BulkSendHelper> bulkSendApp = CreateObject<BulkSendHelper> ("ns3::TcpSocketFactory", sinkAddress);
      bulkSendApp->SetAttribute ("MaxBytes", UintegerValue (0));
      bulkSendApp->SetAttribute ("SendSize", UintegerValue (packetSize));
      ApplicationContainer app = bulkSendApp->Install (centralNode.Get (0));
      app.Start (Seconds (1.0 + 0.1 * i));
      app.Stop (Seconds (simulationTime));

      Ptr<Node> srcNode = centralNode.Get (0);
      Ptr<Ipv4> ipv4 = srcNode->GetObject<Ipv4> ();
      Ptr<NetDevice> dev = deviceContainers[i].Get (0);

      Ptr<StarPacketTracer> tracer = tracers[i];

      Ptr<Socket> tracingSocket = Socket::CreateSocket (centralNode.Get (0), TcpSocketFactory::GetTypeId ());
      tracingSocket->Bind ();
      tracingSocket->Connect (sinkAddress);

      tracingSocket->TraceConnectWithoutContext (
          "Tx", MakeCallback ([tracer](Ptr<const Packet> pkt, const Address &addr) { tracer->LogTx (pkt, addr); }));

      Ptr<Node> dstNode = leafNodes.Get (i);
      Ptr<Socket> sinkSocket = Socket::CreateSocket (dstNode, TcpSocketFactory::GetTypeId ());
      sinkSocket->Bind (InetSocketAddress (interfaces[i].GetAddress (1), port));
      sinkSocket->Listen ();
      sinkSocket->TraceConnectWithoutContext (
          "Rx", MakeCallback ([tracer](Ptr<const Packet> pkt, const Address &addr) { tracer->LogRx (pkt, addr); }));

      sourceApps.Add (app);
    }

  Simulator::Stop (Seconds (simulationTime + 2));
  Simulator::Run ();

  std::ofstream csv (csvFile, std::ofstream::trunc);
  csv << "source,destination,packet_size_bytes,tx_time_sec,rx_time_sec\n";
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      tracers[i]->WriteCsv (csvFile + std::to_string (i));
      std::ifstream in (csvFile + std::to_string (i));
      std::string line;
      bool first = true;
      while (std::getline (in, line))
        {
          if (first)
            {
              first = false;
              continue;
            }
          csv << line << '\n';
        }
      in.close ();
      remove ((csvFile + std::to_string (i)).c_str ());
    }
  csv.close ();

  Simulator::Destroy ();
  return 0;
}