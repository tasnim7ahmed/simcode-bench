#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>

using namespace ns3;

struct PacketRecord
{
  uint32_t src;
  uint32_t dst;
  uint32_t size;
  double txTime;
  double rxTime;
};

class StarPacketTracer
{
public:
  void AddTx (Ptr<const Packet> pkt, uint32_t src, uint32_t dst)
  {
    uint64_t pid = pkt->GetUid();
    double now = Simulator::Now().GetSeconds();
    m_txMap[pid] = {src, dst, pkt->GetSize(), now, 0.0};
  }

  void AddRx (Ptr<const Packet> pkt)
  {
    uint64_t pid = pkt->GetUid();
    double now = Simulator::Now().GetSeconds();
    auto it = m_txMap.find(pid);
    if (it != m_txMap.end())
    {
      it->second.rxTime = now;
      m_records.push_back(it->second);
      m_txMap.erase(it);
    }
  }

  void WriteCsv (const std::string &filename)
  {
    std::ofstream os(filename, std::ios::out);
    os << "src,dst,packet_size,tx_time,rx_time\n";
    os << std::fixed << std::setprecision(9);
    for (const auto &rec : m_records)
    {
      os << rec.src << ","
         << rec.dst << ","
         << rec.size << ","
         << rec.txTime << ","
         << rec.rxTime << "\n";
    }
    os.close();
  }
private:
  std::map<uint64_t, PacketRecord> m_txMap;
  std::vector<PacketRecord> m_records;
};

StarPacketTracer g_tracer;

void TxTrace (Ptr<const Packet> pkt, const Address &srcAddress)
{
  // Find src node by looking up socket owner
  Ptr<Node> srcNode = nullptr;
  Address realAddr = srcAddress;
  if (InetSocketAddress::IsMatchingType(realAddr))
  {
    Ipv4Address ip = InetSocketAddress::ConvertFrom(realAddr).GetIpv4();
    NodeContainer nodes = NodeContainer::GetGlobal();
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
      for (uint32_t j = 1; j <= ipv4->GetNInterfaces(); ++j)
      {
        for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k)
        {
          if (ipv4->GetAddress(j, k).GetLocal() == ip)
            srcNode = nodes.Get(i);
        }
      }
    }
  }
  // Store src and dst, we'll fill dst in at installation
  uint32_t srcIdx = 0;
  if (srcNode)
    srcIdx = srcNode->GetId();
  // We cannot know dst node here, so we'll fixup with a map based on ports below
  // For this demo, dst node is known at Application install
  // We'll use a global map from src TCP port to dst
}

void AppTxTrace (Ptr<const Packet> pkt, const Address &src, uint32_t srcIdx, uint32_t dstIdx)
{
  g_tracer.AddTx(pkt, srcIdx, dstIdx);
}

void RxTrace (Ptr<const Packet> pkt, const Address &address)
{
  g_tracer.AddRx(pkt);
}

class TracedPacketSink : public PacketSink
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TracedPacketSink")
      .SetParent<PacketSink> ()
      .AddConstructor<TracedPacketSink> ();
    return tid;
  }
  virtual void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
    {
      PacketSink::HandleRead (socket);
      RxTrace(packet, from);
    }
  }
};

int main (int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  NodeContainer nodes;
  nodes.Create(5);

  InternetStackHelper stack;
  stack.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  std::vector<NetDeviceContainer> devices(4);
  std::vector<NodeContainer> linkNodes(4);
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interfaces(4);

  for (uint32_t i = 0; i < 4; ++i)
  {
    linkNodes[i] = NodeContainer(nodes.Get(0), nodes.Get(i+1));
    devices[i] = p2p.Install(linkNodes[i]);
    std::ostringstream subnet;
    subnet << "10.1." << i+1 << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = ipv4.Assign(devices[i]);
  }

  uint16_t basePort = 4000;
  ApplicationContainer sinkApps;

  // PacketSink on nodes 1..4 (peripherals)
  for (uint32_t i = 1; i < 5; ++i)
  {
    Address sinkAddress (InetSocketAddress(interfaces[i-1].GetAddress(1), basePort + i - 1));
    Ptr<TracedPacketSink> sink = CreateObject<TracedPacketSink>();
    sink->Setup(InetSocketAddress(interfaces[i-1].GetAddress(1), basePort + i - 1), 0);
    nodes.Get(i)->AddApplication(sink);
    sink->SetStartTime(Seconds(0.0));
    sink->SetStopTime(Seconds(10.0));
  }

  // BulkSendApp on node 0 to each peripheral
  ApplicationContainer sourceApps;
  for (uint32_t i = 1; i < 5; ++i)
  {
    BulkSendHelper source ("ns3::TcpSocketFactory",
                           InetSocketAddress(interfaces[i-1].GetAddress(1), basePort + i - 1));
    source.SetAttribute("MaxBytes", UintegerValue(50 * 1024));
    source.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer app = source.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(9.0));

    // Trace each BulkSendApp
    Ptr<Application> appPtr = app.Get(0);
    Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication>(appPtr);

    bulkApp->TraceConnectWithoutContext("Tx", MakeBoundCallback(&AppTxTrace, 0, i));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  g_tracer.WriteCsv("star_dataset.csv");

  Simulator::Destroy();
  return 0;
}