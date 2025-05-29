#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnCsmaUdp");

class MetricsCollector
{
public:
  MetricsCollector(uint32_t numSenders)
    : m_numSenders(numSenders), m_sent(0), m_received(0), m_totalDelay(Seconds(0.0)) {}

  void SentPacket()
  {
    ++m_sent;
  }

  void ReceivedPacket(Ptr<const Packet> pkt, const Address &from)
  {
    uint64_t uid = pkt->GetUid();
    Time now = Simulator::Now();
    auto it = m_sendTimes.find(uid);
    if (it != m_sendTimes.end())
    {
      Time delay = now - it->second;
      m_totalDelay += delay;
      ++m_received;
      m_sendTimes.erase(it);
    }
  }

  void RegisterSendTime (Ptr<Packet> pkt)
  {
    m_sendTimes[pkt->GetUid()] = Simulator::Now();
  }

  void Report()
  {
    double pdr = (m_sent == 0) ? 0.0 : (double)m_received / (double)m_sent;
    double avgDelay = (m_received == 0) ? 0.0 : m_totalDelay.GetSeconds() / m_received;
    std::cout << "Packets sent:     " << m_sent << std::endl;
    std::cout << "Packets received: " << m_received << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
  }

private:
  uint32_t m_numSenders;
  uint32_t m_sent;
  uint32_t m_received;
  Time m_totalDelay;
  std::map<uint64_t, Time> m_sendTimes;
};

MetricsCollector metrics(5);

void GenerateTraffic(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port, uint32_t pktSize,
                     uint32_t numPkts, Time pktInterval)
{
  if (numPkts > 0)
  {
    Ptr<Packet> pkt = Create<Packet>(pktSize);
    metrics.RegisterSendTime(pkt);
    socket->SendTo(pkt, 0, InetSocketAddress(dstAddr, port));
    metrics.SentPacket();
    Simulator::Schedule(pktInterval, &GenerateTraffic, socket, dstAddr, port, pktSize, numPkts - 1, pktInterval);
  }
}

void ReceivePacket(Ptr<Socket> socket)
{
  while (Ptr<Packet> pkt = socket->Recv())
  {
    metrics.ReceivedPacket(pkt, socket->GetSockName());
  }
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 6;
  uint32_t baseStation = 0;
  uint16_t udpPort = 4000;
  uint32_t pktSize = 40;
  uint32_t numPackets = 30;
  double pktInterval = 2.0;

  CommandLine cmd;
  cmd.AddValue("pktInterval", "Interval between packets sent (s)", pktInterval);
  cmd.AddValue("numPackets", "Number of packets to send from each sensor", numPackets);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("2Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

  NetDeviceContainer devices = csma.Install(nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (20.0),
                                "DeltaY", DoubleValue (20.0),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create base station UDP receiver
  Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(baseStation), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), udpPort);
  recvSink->Bind(local);
  recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

  // Sensor nodes traffic generation
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    if (i == baseStation) continue;
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(baseStation), udpPort);
    srcSocket->Connect(remote);
    Simulator::ScheduleWithContext(srcSocket->GetNode()->GetId(), Seconds(1.0 + 0.1 * i),
                                   &GenerateTraffic, srcSocket,
                                   interfaces.GetAddress(baseStation), udpPort, pktSize, numPackets, Seconds(pktInterval));
  }

  AnimationInterface anim("wsn-csma-udp.xml");
  anim.SetBackgroundImage("background.png", 0, 0, 1.0, 1.0, 1);
  anim.SetConstantPosition(nodes.Get(baseStation), 0, 0);

  Simulator::Stop(Seconds(1.0 + numPackets * pktInterval + 2));
  Simulator::Run();
  metrics.Report();
  Simulator::Destroy();
  return 0;
}