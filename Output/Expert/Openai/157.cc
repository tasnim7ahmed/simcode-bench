#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

struct PacketInfo
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t packetSize;
  double txTime;
  double rxTime;
};

std::vector<PacketInfo> g_packetDataset;
std::map<uint64_t, double> g_packetTxTime;
std::map<uint64_t, std::pair<uint32_t, uint32_t>> g_packetFlow;

void TxTrace(Ptr<const Packet> packet, const Address &address, uint32_t nodeId, uint32_t dstId)
{
  uint64_t uid = packet->GetUid();
  double time = Simulator::Now().GetSeconds();
  g_packetTxTime[uid] = time;
  g_packetFlow[uid] = std::make_pair(nodeId, dstId);
}

void RxTrace(Ptr<const Packet> packet, const Address &address, uint32_t nodeId)
{
  uint64_t uid = packet->GetUid();
  double rxTime = Simulator::Now().GetSeconds();
  auto itTx = g_packetTxTime.find(uid);
  auto itFlow = g_packetFlow.find(uid);
  if (itTx != g_packetTxTime.end() && itFlow != g_packetFlow.end())
  {
    PacketInfo info;
    info.srcNode = itFlow->second.first;
    info.dstNode = itFlow->second.second;
    info.packetSize = packet->GetSize();
    info.txTime = itTx->second;
    info.rxTime = rxTime;
    g_packetDataset.push_back(info);

    g_packetTxTime.erase(itTx);
    g_packetFlow.erase(itFlow);
  }
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 4;
  uint32_t packetSize = 512;
  double totalTime = 5.0;
  double startTime = 1.0;
  double stopTime = totalTime;
  std::string dataRate = "10Mbps";
  std::string delay = "2ms";
  std::string csvFileName = "ring-packet-data.csv";

  CommandLine cmd;
  cmd.AddValue("output", "Output CSV file", csvFileName);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  std::vector<NetDeviceContainer> deviceContainers;
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    NetDeviceContainer ndc = p2p.Install(nodes.Get(i), nodes.Get((i+1)%nNodes));
    deviceContainers.push_back(ndc);
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> ifContainers;
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    std::ostringstream subnet;
    subnet << "10.1." << i+1 << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    ifContainers.push_back(address.Assign(deviceContainers[i]));
  }

  // Each node sends UDP packets to its next ring neighbor
  uint16_t port = 4000;
  ApplicationContainer allApps;
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    uint32_t srcNode = i;
    uint32_t dstNode = (i+1) % nNodes;
    Ptr<Node> src = nodes.Get(srcNode);
    Ptr<Node> dst = nodes.Get(dstNode);

    Ipv4Address dstAddr = ifContainers[dstNode].GetAddress(0);

    // Receiver on dstNode
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(dst);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(stopTime));
    allApps.Add(serverApp);

    // Sender on srcNode
    UdpClientHelper client(dstAddr, port);
    client.SetAttribute("MaxPackets", UintegerValue(50));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(src);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(stopTime));
    allApps.Add(clientApp);

    // Packet send trace
    Ptr<Socket> srcSocket = Socket::CreateSocket(src, UdpSocketFactory::GetTypeId());
    srcSocket->Bind();
    srcSocket->Connect(InetSocketAddress(dstAddr, port));
    srcSocket = 0; // not used further

    Config::Connect(
      "/NodeList/" + std::to_string(srcNode) +
      "/ApplicationList/*/$ns3::UdpClient/Tx",
      MakeBoundCallback(&TxTrace, srcNode, dstNode));

    Config::Connect(
      "/NodeList/" + std::to_string(dstNode) +
      "/ApplicationList/*/$ns3::UdpServer/Rx",
      MakeBoundCallback(&RxTrace, dstNode));
  }

  Simulator::Stop(Seconds(stopTime + 1));
  Simulator::Run();
  Simulator::Destroy();

  std::ofstream csv(csvFileName);
  csv << "Source,Destination,PacketSize,TxTime,RxTime\n";
  for (const auto &info : g_packetDataset)
  {
    csv << info.srcNode << "," << info.dstNode << "," << info.packetSize << ",";
    csv << info.txTime << "," << info.rxTime << "\n";
  }
  csv.close();

  return 0;
}