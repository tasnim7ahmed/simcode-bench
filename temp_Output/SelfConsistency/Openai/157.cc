#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

// Structure to save CSV data
struct PacketInfo
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t packetSize;
  double txTime;
  double rxTime;
};

std::vector<PacketInfo> g_packetInfos;

// Map packet UIDs to transmission info
std::map<uint64_t, std::pair<uint32_t, double>> g_txTrace; // UID -> (srcNodeId, txTime)
std::map<uint64_t, std::pair<uint32_t, uint32_t>> g_uidToSrcDst; // UID -> (srcNode, dstNode)
std::map<uint64_t, uint32_t> g_uidToSize; // UID -> size

void
TxTrace(std::string context, Ptr<const Packet> packet)
{
  // context: "/NodeList/i/ApplicationList/j/$ns3::UdpSocket/Tx"
  uint64_t uid = packet->GetUid();
  uint32_t nodeId = Simulator::GetContext();

  double txTime = Simulator::Now().GetSeconds();

  g_txTrace[uid] = std::make_pair(nodeId, txTime);
  g_uidToSize[uid] = packet->GetSize();
}

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  uint64_t uid = packet->GetUid();

  double rxTime = Simulator::Now().GetSeconds();
  uint32_t rxNodeId = Simulator::GetContext();

  auto txIter = g_txTrace.find(uid);
  if (txIter != g_txTrace.end())
    {
      uint32_t srcNode = txIter->second.first;
      double txTime = txIter->second.second;
      uint32_t dstNode = rxNodeId;
      uint32_t size = g_uidToSize[uid];

      PacketInfo info = {srcNode, dstNode, size, txTime, rxTime};
      g_packetInfos.push_back(info);

      g_txTrace.erase(txIter);
      g_uidToSize.erase(uid);
    }
}

void WriteCsv(const std::string &filename)
{
  std::ofstream ofs(filename);
  ofs << "src_node,dst_node,packet_size,tx_time,rx_time" << std::endl;
  ofs << std::fixed << std::setprecision(9);

  for (const auto &info : g_packetInfos)
    {
      ofs << info.srcNode << "," << info.dstNode << "," << info.packetSize << "," << info.txTime << "," << info.rxTime << std::endl;
    }

  ofs.close();
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 4;
  uint32_t packetSize = 512;
  double interval = 1.0;
  uint32_t numPackets = 10;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("interval", "Inter-packet interval (s)", interval);
  cmd.AddValue("numPackets", "Number of packets per flow", numPackets);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  // Create a ring of point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[numNodes];
  NodeContainer pair;
  Ipv4InterfaceContainer interfaces;

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  char subnet[20];

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      uint32_t from = i;
      uint32_t to = (i + 1) % numNodes;
      pair = NodeContainer(nodes.Get(from), nodes.Get(to));
      devices[i] = p2p.Install(pair);
      sprintf(subnet, "10.%u.1.0", i+1);
      address.SetBase(subnet, "255.255.255.0");
      Ipv4InterfaceContainer iface = address.Assign(devices[i]);
      interfaces.Add(iface);
    }

  // Install UDP applications: node[i] -> node[(i+1)%numNodes]
  uint16_t basePort = 9000;
  ApplicationContainer apps;

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      uint32_t src = i;
      uint32_t dst = (i + 1) % numNodes;
      Ipv4Address dstAddr = interfaces.GetAddress(2 * dst); // Each link has two interfaces, get the "dst" side

      // UDP server (sink) on dst
      UdpServerHelper server(basePort + src);
      apps.Add(server.Install(nodes.Get(dst)));

      // UDP client on src
      UdpClientHelper client(dstAddr, basePort + src);
      client.SetAttribute("MaxPackets", UintegerValue(numPackets));
      client.SetAttribute("Interval", TimeValue(Seconds(interval)));
      client.SetAttribute("PacketSize", UintegerValue(packetSize));
      apps.Add(client.Install(nodes.Get(src)));
    }

  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(1.0 + numPackets * interval + 2)); // give time for last packet to be received

  // Trace transmit and receive at socket level
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpSocket/Tx", MakeCallback(&TxTrace));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpSocket/Rx", MakeCallback(&RxTrace));

  Simulator::Stop(Seconds(1.0 + numPackets * interval + 5));
  Simulator::Run();

  WriteCsv("ring_udp_dataset.csv");

  Simulator::Destroy();
  return 0;
}