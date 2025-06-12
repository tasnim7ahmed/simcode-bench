#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <sstream>

using namespace ns3;

struct PacketLogEntry
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t size;
  double txTime;
  double rxTime;
};

std::vector<PacketLogEntry> g_packetLog;

class MyApp : public Application
{
public:
  MyApp() {}
  virtual ~MyApp() {}

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, uint32_t myNodeId, uint32_t destNodeId)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_pktInterval = pktInterval;
    m_sent = 0;
    m_myNodeId = myNodeId;
    m_destNodeId = destNodeId;
  }

private:
  virtual void StartApplication(void)
  {
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
      m_socket->Close();
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    // Store transmission time and per-packet metadata in tags
    Time txTime = Simulator::Now();
    struct Metadata : public Tag
    {
      uint32_t srcNode;
      uint32_t dstNode;
      double txTime;
      static TypeId GetTypeId (void)
      {
        static TypeId tid = TypeId ("Metadata")
          .SetParent<Tag> ()
          .AddConstructor<Metadata> ();
        return tid;
      }
      virtual TypeId GetInstanceTypeId (void) const
      {
        return GetTypeId ();
      }
      virtual uint32_t GetSerializedSize (void) const { return sizeof(uint32_t)*2 + sizeof(double);}
      virtual void Serialize (TagBuffer i) const
      {
        i.WriteU32 (srcNode);
        i.WriteU32 (dstNode);
        i.Write ((const uint8_t *)&txTime, sizeof(double));
      }
      virtual void Deserialize (TagBuffer i)
      {
        srcNode = i.ReadU32 ();
        dstNode = i.ReadU32 ();
        i.Read ((uint8_t *)&txTime, sizeof(double));
      }
      virtual void Print (std::ostream &os) const
      {
        os << "srcNode=" << srcNode << " dstNode=" << dstNode << " txTime=" << txTime;
      }
    } tag;
    tag.srcNode = m_myNodeId;
    tag.dstNode = m_destNodeId;
    tag.txTime = txTime.GetSeconds();
    packet->AddPacketTag(tag);

    m_socket->Send(packet);
    m_sent++;
    if (m_sent < m_nPackets)
    {
      Simulator::Schedule(m_pktInterval, &MyApp::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_pktInterval;
  uint32_t m_sent;
  uint32_t m_myNodeId;
  uint32_t m_destNodeId;
};

// Receive callback
void RxCallback(Ptr<Socket> socket)
{
  Address srcAddress;
  while (Ptr<Packet> packet = socket->RecvFrom(srcAddress))
  {
    // Retrieve metadata tag from packet
    struct Metadata : public Tag
    {
      uint32_t srcNode;
      uint32_t dstNode;
      double txTime;
      static TypeId GetTypeId (void)
      {
        static TypeId tid = TypeId ("Metadata")
          .SetParent<Tag> ()
          .AddConstructor<Metadata> ();
        return tid;
      }
      virtual TypeId GetInstanceTypeId (void) const
      {
        return GetTypeId ();
      }
      virtual uint32_t GetSerializedSize (void) const { return sizeof(uint32_t)*2 + sizeof(double);}
      virtual void Serialize (TagBuffer i) const
      {
        i.WriteU32 (srcNode);
        i.WriteU32 (dstNode);
        i.Write ((const uint8_t *)&txTime, sizeof(double));
      }
      virtual void Deserialize (TagBuffer i)
      {
        srcNode = i.ReadU32 ();
        dstNode = i.ReadU32 ();
        i.Read ((uint8_t *)&txTime, sizeof(double));
      }
      virtual void Print (std::ostream &os) const
      {
        os << "srcNode=" << srcNode << " dstNode=" << dstNode << " txTime=" << txTime;
      }
    } tag;
    if (packet->PeekPacketTag(tag))
    {
      double rxTime = Simulator::Now().GetSeconds();
      PacketLogEntry e;
      e.srcNode = tag.srcNode;
      e.dstNode = tag.dstNode;
      e.size = packet->GetSize();
      e.txTime = tag.txTime;
      e.rxTime = rxTime;
      g_packetLog.push_back(e);
    }
  }
}

void WriteCsv()
{
  std::ofstream csv("packet_log.csv");
  csv << "srcNode,dstNode,packetSize,txTime,rxTime\n";
  for (const auto& e : g_packetLog)
  {
    csv << e.srcNode << "," << e.dstNode << "," << e.size << "," << e.txTime << "," << e.rxTime << "\n";
  }
  csv.close();
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 4;
  uint32_t packetSize = 512;
  uint32_t nPackets = 10;
  double interval = 1.0; // seconds between packets
  double simTime = 12.0; // enough time for all packets

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("interval", "Inter-packet interval (s)", interval);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  // Build ring topology
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[nNodes];
  Ipv4InterfaceContainer interfaces[nNodes];

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  std::ostringstream subnet;

  // Link i <-> (i+1)%nNodes for ring
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    NodeContainer pair;
    pair.Add(nodes.Get(i));
    pair.Add(nodes.Get((i+1)%nNodes));
    devices[i] = p2p.Install(pair);

    subnet.str("");
    subnet << "10.1." << i+1 << ".0";
    address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
    interfaces[i] = address.Assign(devices[i]);
  }

  // Install UDP applications: node i -> node (i+1)%nNodes
  uint16_t port = 9000;
  ApplicationContainer apps;

  for (uint32_t i = 0; i < nNodes; ++i)
  {
    uint32_t dst = (i+1)%nNodes;
    // Sink on dst
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(dst), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&RxCallback));

    // Sender on src
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(i), tid);
    InetSocketAddress remote = InetSocketAddress(interfaces[i].GetAddress(1), port);

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(srcSocket, remote, packetSize, nPackets, Seconds(interval), i, dst);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1.0)); // stagger to avoid startup issues
    app->SetStopTime(Seconds(simTime-1));
    apps.Add(app);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  WriteCsv();

  Simulator::Destroy();
  return 0;
}