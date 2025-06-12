#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

struct PacketRecord
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t pktSize;
  double txTime;
  double rxTime;
};

class RingDatasetCollector
{
public:
  RingDatasetCollector(std::string filename)
  {
    m_out.open(filename);
    m_out << "srcNode,dstNode,packetSize,txTime,rxTime\n";
  }
  ~RingDatasetCollector()
  {
    m_out.close();
  }

  void TxTrace(Ptr<const Packet> pkt, const Address &address)
  {
    uint64_t uid = pkt->GetUid();
    double time = Simulator::Now().GetSeconds();
    m_txRecords[uid] = time;
  }
  void RxTrace(Ptr<const Packet> pkt, const Address &address)
  {
    uint64_t uid = pkt->GetUid();
    double rxTime = Simulator::Now().GetSeconds();

    auto i = m_txRecords.find(uid);
    if (i != m_txRecords.end())
    {
      double txTime = i->second;
      uint32_t srcNode = GetNodeIdFromAddress(pkt, true);
      uint32_t dstNode = GetNodeIdFromAddress(pkt, false);
      m_out << srcNode << "," << dstNode << ","
            << pkt->GetSize() << ","
            << std::fixed << std::setprecision(6) << txTime << ","
            << std::fixed << std::setprecision(6) << rxTime << "\n";
      m_txRecords.erase(i);
    }
  }

  static uint32_t GetNodeIdFromAddress(Ptr<const Packet> pkt, bool isSrc)
  {
    // This simulation assumes one flow per app: src/dst are known by context
    return 0; // Placeholder, will be overridden by bound context
  }

  void SetBinding(uint32_t src, uint32_t dst)
  {
    m_srcNode = src;
    m_dstNode = dst;
  }

  void BoundTxTrace(Ptr<const Packet> pkt, const Address &address)
  {
    uint64_t uid = pkt->GetUid();
    double time = Simulator::Now().GetSeconds();
    m_txRecords[uid] = time;
    m_uid2src[uid] = m_srcNode;
    m_uid2dst[uid] = m_dstNode;
  }

  void BoundRxTrace(Ptr<const Packet> pkt, const Address &address)
  {
    uint64_t uid = pkt->GetUid();
    double rxTime = Simulator::Now().GetSeconds();
    auto i = m_txRecords.find(uid);
    if (i != m_txRecords.end())
    {
      double txTime = i->second;
      uint32_t srcNode = m_uid2src[uid];
      uint32_t dstNode = m_uid2dst[uid];
      m_out << srcNode << "," << dstNode << ","
            << pkt->GetSize() << ","
            << std::fixed << std::setprecision(6) << txTime << ","
            << std::fixed << std::setprecision(6) << rxTime << "\n";
      m_txRecords.erase(i);
      m_uid2src.erase(uid);
      m_uid2dst.erase(uid);
    }
  }

private:
  std::ofstream m_out;
  std::map<uint64_t, double> m_txRecords;
  std::map<uint64_t, uint32_t> m_uid2src, m_uid2dst;
  uint32_t m_srcNode, m_dstNode;
};

int main(int argc, char *argv[])
{
  uint32_t numNodes = 4;
  std::string dataRate = "10Mbps";
  std::string delay = "2ms";
  uint32_t packetSize = 512;
  double simTime = 10.0;
  uint32_t pktRate = 5;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of each packet", packetSize);
  cmd.AddValue("pktRate", "Packets per second", pktRate);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  // Each node connects to next in ring, wrap around from last to first
  std::vector<NetDeviceContainer> devices;
  std::vector<PointToPointHelper> p2pHelpers;

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;
  char subnet[20];

  for (uint32_t i = 0; i < numNodes; ++i)
  {
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));
    p2pHelpers.push_back(p2p);

    uint32_t next = (i + 1) % numNodes;
    NetDeviceContainer dev = p2p.Install(NodeContainer(nodes.Get(i), nodes.Get(next)));
    devices.push_back(dev);

    // Assign subnet per-point-to-point link
    sprintf(subnet, "10.1.%u.0", i + 1);
    address.SetBase(Ipv4Address(subnet), "255.255.255.0");
    interfaces.push_back(address.Assign(dev));
  }

  // Set up UDP traffic: Node i sends to Node (i+1) (one direction per link)
  uint16_t basePort = 8080;
  ApplicationContainer allApps;

  // Set up dataset collector
  RingDatasetCollector collector("ring-dataset.csv");

  for (uint32_t i = 0; i < numNodes; ++i)
  {
    uint32_t srcIdx = i;
    uint32_t dstIdx = (i + 1) % numNodes;

    // Install UDP server at dstIdx
    UdpServerHelper server(basePort + srcIdx);
    ApplicationContainer serverApp = server.Install(nodes.Get(dstIdx));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Configure UDP client: sends to neighboring node's address on their "incoming" interface
    UdpClientHelper client(interfaces[i].GetAddress(1), basePort + srcIdx);
    client.SetAttribute("MaxPackets", UintegerValue(pktRate * simTime));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0 / pktRate)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(nodes.Get(srcIdx));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    allApps.Add(serverApp);
    allApps.Add(clientApp);

    // Instrument for packet tracing (Tx at client, Rx at server)
    Ptr<Node> srcNode = nodes.Get(srcIdx);
    Ptr<Node> dstNode = nodes.Get(dstIdx);

    Ptr<Application> app = clientApp.Get(0);
    Ptr<Socket> clientSock = DynamicCast<UdpClient>(app)->GetSocket();
    // Bind with context
    collector.SetBinding(srcIdx, dstIdx);
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(srcIdx) +
                                  "/ApplicationList/0/$ns3::UdpClient/Tx",
                                  MakeCallback(&RingDatasetCollector::BoundTxTrace, &collector));
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(dstIdx) +
                                  "/ApplicationList/0/$ns3::UdpServer/Rx",
                                  MakeCallback(&RingDatasetCollector::BoundRxTrace, &collector));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}