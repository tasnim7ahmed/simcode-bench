#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopologyDataset");

static std::ofstream dataFile;

class TcpPacketTracer
{
public:
  static void
  TxPacket (Ptr<const Packet> packet, const Address &address, uint32_t nodeId, uint32_t dstNodeId)
  {
    double time = Simulator::Now().GetSeconds();
    uint32_t pktSize = packet->GetSize();
    uint64_t packetId = packet->GetUid();
    m_txTimes[packetId] = time;
    m_srcs[packetId] = nodeId;
    m_dsts[packetId] = dstNodeId;
    // No file output now, wait for Rx
  }

  static void
  RxPacket (Ptr<const Packet> packet, const Address &address, uint32_t nodeId)
  {
    double time = Simulator::Now().GetSeconds();
    uint64_t packetId = packet->GetUid();
    auto txIt = m_txTimes.find(packetId);
    auto srcIt = m_srcs.find(packetId);
    auto dstIt = m_dsts.find(packetId);
    if (txIt != m_txTimes.end() && srcIt != m_srcs.end() && dstIt != m_dsts.end())
      {
        double txTime = txIt->second;
        uint32_t src = srcIt->second;
        uint32_t dst = dstIt->second;
        uint32_t pktSize = packet->GetSize();
        dataFile << src << "," << nodeId << "," << pktSize << "," << txTime << "," << time << std::endl;
        m_txTimes.erase(packetId);
        m_srcs.erase(packetId);
        m_dsts.erase(packetId);
      }
  }

  static void
  TrackTx (Ptr<Socket> socket, uint32_t nodeId, uint32_t dstNodeId)
  {
    socket->TraceConnectWithoutContext(
      "Tx", MakeBoundCallback(&TcpPacketTracer::TcpSocketTx, nodeId, dstNodeId));
  }

  static void
  TrackRx (Ptr<Socket> socket, uint32_t nodeId)
  {
    socket->TraceConnectWithoutContext(
      "Rx", MakeBoundCallback(&TcpPacketTracer::TcpSocketRx, nodeId));
  }

private:
  static void
  TcpSocketTx (uint32_t nodeId, uint32_t dstNodeId, Ptr<const Packet> packet)
  {
    // For each Tx, record the time and src/dst
    TcpPacketTracer::TxPacket(packet, Address(), nodeId, dstNodeId);
  }

  static void
  TcpSocketRx (uint32_t nodeId, Ptr<const Packet> packet, const Address &address)
  {
    TcpPacketTracer::RxPacket(packet, address, nodeId);
  }

  static std::map<uint64_t, double> m_txTimes;
  static std::map<uint64_t, uint32_t> m_srcs;
  static std::map<uint64_t, uint32_t> m_dsts;
};

std::map<uint64_t, double> TcpPacketTracer::m_txTimes;
std::map<uint64_t, uint32_t> TcpPacketTracer::m_srcs;
std::map<uint64_t, uint32_t> TcpPacketTracer::m_dsts;

int
main (int argc, char *argv[])
{
  uint32_t numSpokes = 4;
  double simTime = 10.0;
  srand (time (0));

  dataFile.open("tcp_star_dataset.csv", std::ios_base::out);
  dataFile << "Src,Dst,PacketSize,TxTime,RxTime" << std::endl;

  NodeContainer nodes;
  nodes.Create(numSpokes + 1); // Node 0: center, nodes 1-4: spokes

  std::vector<NodeContainer> starLinks;
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      NodeContainer link (nodes.Get(0), nodes.Get(i + 1));
      starLinks.push_back(link);
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  std::vector<NetDeviceContainer> deviceContainers;
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      deviceContainers.push_back(p2p.Install(starLinks[i]));
    }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i+1) << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      interfaces.push_back(address.Assign(deviceContainers[i]));
    }

  uint16_t basePort = 5000;
  uint32_t packetSize = 1024;
  double appStart = 1.0;
  double appStop = simTime - 1.0;

  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      Address sinkAddress (InetSocketAddress (interfaces[i].GetAddress(1), basePort + i));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort + i));
      ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i+1));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(simTime));
    }

  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      Ptr<Socket> tcpSocket = Socket::CreateSocket (nodes.Get(0), TcpSocketFactory::GetTypeId ());
      Ptr<Application> app = CreateObject<BulkSendApplication> ();
      app->SetAttribute("Socket", PointerValue(tcpSocket));
      app->SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces[i].GetAddress(1), basePort + i)));
      app->SetAttribute("SendSize", UintegerValue(packetSize));
      app->SetAttribute("MaxBytes", UintegerValue(20 * packetSize));
      nodes.Get(0)->AddApplication(app);
      app->SetStartTime(Seconds(appStart+0.2*i));
      app->SetStopTime(Seconds(appStop));

      // Trace on TCP TX for each source-dest pair
      TcpPacketTracer::TrackTx(tcpSocket, 0, i+1);

      // Trace on RX at each sink
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(nodes.Get(i+1)->GetApplication(0));
      if (sink)
        {
          Ptr<Socket> sinkSocket = sink->GetSocket();
          TcpPacketTracer::TrackRx(sinkSocket, i+1);
        }
    }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  dataFile.close();
  return 0;
}