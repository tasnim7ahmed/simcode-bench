#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTcpDatasetExample");

struct PacketRecord {
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t size;
  double txTime;
  double rxTime;
};

class DatasetTracer {
public:
  void Tx (Ptr<const Packet> packet, const Address &address, uint32_t srcId, uint32_t dstId) {
    PacketInfo info;
    info.packetUid = packet->GetUid();
    info.srcNode = srcId;
    info.dstNode = dstId;
    info.size = packet->GetSize();
    info.txTime = Simulator::Now().GetSeconds();
    txPackets[info.packetUid] = info;
  }

  void Rx (Ptr<const Packet> packet, uint32_t dstId) {
    uint64_t uid = packet->GetUid();
    auto it = txPackets.find(uid);
    if (it != txPackets.end()) {
      PacketRecord record;
      record.srcNode = it->second.srcNode;
      record.dstNode = it->second.dstNode;
      record.size = it->second.size;
      record.txTime = it->second.txTime;
      record.rxTime = Simulator::Now().GetSeconds();
      records.push_back(record);
      txPackets.erase(it);
    }
  }

  void WriteCsv (const std::string &filename) {
    std::ofstream out(filename, std::ios::out);
    out << "src_node,dst_node,packet_size,tx_time,rx_time\n";
    for (const auto &rec : records) {
      out << rec.srcNode << "," << rec.dstNode << "," << rec.size << "," << rec.txTime << "," << rec.rxTime << "\n";
    }
    out.close();
  }

private:
  struct PacketInfo {
    uint64_t packetUid;
    uint32_t srcNode;
    uint32_t dstNode;
    uint32_t size;
    double txTime;
  };
  std::map<uint64_t, PacketInfo> txPackets;
  std::vector<PacketRecord> records;
};

DatasetTracer g_tracer;

// Custom OnOffApplication to expose destination in Tx trace
class StarOnOffApp : public OnOffApplication {
public:
  static TypeId GetTypeId(void) {
    static TypeId tid = TypeId("StarOnOffApp")
      .SetParent<OnOffApplication>()
      .AddConstructor<StarOnOffApp>();
    return tid;
  }
  StarOnOffApp() {}
  virtual ~StarOnOffApp() {}

protected:
  virtual void SendPacket() {
    OnOffApplication::SendPacket();
    Ptr<Socket> socket = GetSocket();
    if (!socket) return;
    Ptr<Node> node = GetNode();
    uint32_t srcId = node->GetId();
    // Record dest from m_peer
    InetSocketAddress addr = InetSocketAddress::ConvertFrom(m_peer);
    uint32_t dstId = 0;
    for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i) {
      Ptr<Node> n = NodeList::GetNode(i);
      for (uint32_t j = 0; j < n->GetNDevices(); ++j) {
        Ptr<NetDevice> dev = n->GetDevice(j);
        Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
        for (uint32_t iface = 0; iface < ipv4->GetNInterfaces(); ++iface) {
          for (uint32_t addrIdx = 0; addrIdx < ipv4->GetNAddresses(iface); ++addrIdx) {
            Ipv4Address ip = ipv4->GetAddress(iface, addrIdx).GetLocal();
            if (ip == addr.GetIpv4()) {
              dstId = n->GetId();
            }
          }
        }
      }
    }
    g_tracer.Tx(m_lastPkt, m_peer, srcId, dstId);
  }
};

NS_OBJECT_ENSURE_REGISTERED(StarOnOffApp);

void
RxTrace (Ptr<const Packet> packet, const Address &address, uint32_t dstId)
{
  g_tracer.Rx(packet, dstId);
}

int main(int argc, char *argv[])
{
  uint32_t nSpokes = 4;
  double simTime = 5.0;
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("1Mbps"));

  NodeContainer nodes;
  nodes.Create (nSpokes + 1); // node 0 central, 1-4 peripheral

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  Ipv4AddressHelper address;
  std::vector<NetDeviceContainer> netDevs;
  std::vector<Ipv4InterfaceContainer> interfaces;

  InternetStackHelper stack;
  stack.Install (nodes);

  for (uint32_t i = 1; i <= nSpokes; ++i) {
    NetDeviceContainer link = p2p.Install (NodeContainer(nodes.Get(0), nodes.Get(i)));
    netDevs.push_back(link);
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    address.SetBase (Ipv4Address(subnet.str().c_str()), "255.255.255.0");
    interfaces.push_back(address.Assign (link));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install server Applications (PacketSink on peripherals)
  std::vector<uint16_t> sinkPorts = {8001, 8002, 8003, 8004};
  for (uint32_t i = 1; i <= nSpokes; ++i) {
    Address sinkAddr (InetSocketAddress(interfaces[i-1].GetAddress(1), sinkPorts[i-1]));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get(i));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (simTime+1));
    // Trace Rx
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxTrace, sink->GetNode()->GetId()));
  }

  // Install StarOnOffApp (TCP clients) on central node (node 0)
  for (uint32_t i = 1; i <= nSpokes; ++i) {
    Address remoteAddr (InetSocketAddress(interfaces[i-1].GetAddress(1), sinkPorts[i-1]));
    Ptr<StarOnOffApp> app = CreateObject<StarOnOffApp>();
    app->SetAttribute ("Remote", AddressValue (remoteAddr));
    app->SetAttribute ("Protocol", StringValue ("ns3::TcpSocketFactory"));
    app->SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    app->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    nodes.Get(0)->AddApplication (app);
    app->SetStartTime (Seconds (1.0 + 0.2*i)); // Staggered start for diversity
    app->SetStopTime (Seconds (simTime));
  }

  Simulator::Stop(Seconds(simTime+1));
  Simulator::Run();

  g_tracer.WriteCsv ("star_tcp_dataset.csv");

  Simulator::Destroy();
  return 0;
}