#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;
using namespace std;

class PacketEventLogger
{
public:
  PacketEventLogger (std::string filename)
  {
    m_ofstream.open (filename, std::ios::out | std::ios::trunc);
    m_ofstream << "SrcNode,DestNode,PacketSize,TxTime,RxTime\n";
  }

  ~PacketEventLogger ()
  {
    m_ofstream.close ();
  }

  void LogTx (Ptr<const Packet> packet, uint32_t srcNode, uint32_t dstNode)
  {
    uint64_t uid = packet->GetUid ();
    m_txInfo[uid] = TxInfo {srcNode, dstNode, packet->GetSize (), Simulator::Now ()};
  }

  void LogRx (Ptr<const Packet> packet)
  {
    uint64_t uid = packet->GetUid ();
    auto it = m_txInfo.find (uid);
    if (it != m_txInfo.end ())
      {
        m_ofstream << it->second.srcNode << ","
                   << it->second.dstNode << ","
                   << it->second.packetSize << ","
                   << it->second.txTime.GetSeconds () << ","
                   << Simulator::Now ().GetSeconds () << "\n";
        m_ofstream.flush ();
        m_txInfo.erase (it);
      }
  }

private:
  struct TxInfo
  {
    uint32_t srcNode;
    uint32_t dstNode;
    uint32_t packetSize;
    Time txTime;
  };
  std::ofstream m_ofstream;
  std::map<uint64_t, TxInfo> m_txInfo;
};

Ptr<PacketEventLogger> logger;

void
TxTrace (Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
  uint32_t srcNode = ipv4->GetObject<Node> ()->GetId ();
  // The application will set the destination node in the packet's tag.
  // We retrieve it below.
  uint32_t dstNode = 0;
  ByteTagIterator i = packet->GetTagIterator ();
  while (i.HasNext ())
    {
      ByteTagIterator::Item item = i.Next ();
      if (item.GetTypeId ().GetName () == "ns3::UintegerValue")
        {
          UintegerValueTag tag;
          item.Deserialize (&tag, sizeof(UintegerValueTag));
          dstNode = tag.Get ();
          break;
        }
    }
  logger->LogTx (packet, srcNode, dstNode);
}

void
RxTrace (Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
  logger->LogRx (packet);
}

class DestNodeTag : public Tag
{
public:
  DestNodeTag () : m_val (0) {}
  DestNodeTag (uint32_t val) : m_val (val) {}
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("DestNodeTag")
      .SetParent<Tag> ()
      .AddConstructor<DestNodeTag> ();
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const { return GetTypeId (); }
  virtual void Serialize (TagBuffer i) const { i.WriteU32 (m_val); }
  virtual void Deserialize (TagBuffer i) { m_val = i.ReadU32 (); }
  virtual uint32_t GetSerializedSize (void) const { return 4; }
  virtual void Print (std::ostream &os) const { os << m_val; }
  void Set (uint32_t val) { m_val = val; }
  uint32_t Get () const { return m_val; }
private:
  uint32_t m_val;
};

class TaggedPacketSink : public PacketSink
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TaggedPacketSink")
      .SetParent<PacketSink> ()
      .AddConstructor<TaggedPacketSink> ();
    return tid;
  }

  virtual void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        PacketSink::HandleReadOne (socket, packet, from);
        logger->LogRx (packet);
      }
  }
};

class TaggedOnOffApplication : public OnOffApplication
{
public:
  TaggedOnOffApplication () {}

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TaggedOnOffApplication")
      .SetParent<OnOffApplication> ()
      .AddConstructor<TaggedOnOffApplication> ();
    return tid;
  }
  void SetDestNode (uint32_t destNode)
  {
    m_destNode = destNode;
  }
protected:
  virtual void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_pktSize);
    DestNodeTag tag (m_destNode);
    packet->AddPacketTag (tag);
    OnOffApplication::SendPacket (packet);
    logger->LogTx (packet, GetNode ()->GetId (), m_destNode);
  }
private:
  uint32_t m_destNode = 0;
};

int
main (int argc, char *argv[])
{
  logger = CreateObject<PacketEventLogger> ("packet_dataset.csv");

  NodeContainer nodes;
  nodes.Create (5); // Node 0: center, nodes 1-4: peripherals

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devs[4];
  Ipv4InterfaceContainer ifaces[4];
  Ipv4AddressHelper ipa;
  for (uint32_t i = 0; i < 4; ++i)
    {
      NodeContainer pair (nodes.Get (0), nodes.Get (i + 1));
      devs[i] = p2p.Install (pair);
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      ipa.SetBase (subnet.str ().c_str (), "255.255.255.0");
      ifaces[i] = ipa.Assign (devs[i]);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 5000;

  // Install sinks on each peripheral
  ApplicationContainer sinkApps;
  for (uint32_t i = 1; i <= 4; ++i)
    {
      Address sinkAddress (InetSocketAddress (ifaces[i - 1].GetAddress (1), port));
      PacketSinkHelper sink ("ns3::TcpSocketFactory", sinkAddress);
      sinkApps.Add (sink.Install (nodes.Get (i)));
    }

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (12.0));

  // Install TaggedOnOffApplication on the central node for each peripheral
  ApplicationContainer sourceApps;
  for (uint32_t i = 1; i <= 4; ++i)
    {
      Ptr<TaggedOnOffApplication> app = CreateObject<TaggedOnOffApplication> ();
      AddressValue remote (InetSocketAddress (ifaces[i - 1].GetAddress (1), port));
      app->SetAttribute ("Remote", remote);
      app->SetAttribute ("Protocol", StringValue ("ns3::TcpSocketFactory"));
      app->SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      app->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      app->SetAttribute ("DataRate", DataRateValue (DataRate ("2Mbps")));
      app->SetAttribute ("PacketSize", UintegerValue (1024));
      app->SetDestNode (i);
      nodes.Get (0)->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + i));
      app->SetStopTime (Seconds (11.0));
      sourceApps.Add (app);
    }

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}