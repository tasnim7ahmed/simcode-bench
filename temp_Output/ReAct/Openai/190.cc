#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Manet3NodeSimulation");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void RxPacket (Ptr<const Packet> packet, const Address & senderAddress, const Address & receiverAddress)
{
  packetsReceived++;
}

void RxTrace (Ptr<const Packet> packet, const Address & senderAddress, const Address & receiverAddress)
{
  packetsReceived++;
}

void TxTrace (Ptr<const Packet> packet)
{
  packetsSent++;
}

void PacketRxTracer (Ptr<const Packet> p, const Address &addr)
{
  packetsReceived++;
}

void PacketDelivery (Ptr<const Packet> p, const Address &addr)
{
  packetsReceived++;
}

void RxWithTimestamp (Ptr<const Packet> packet, const Address & from, const Address & to)
{
  packetsReceived++;
}

void RxTraceWithDelay (Ptr<const Packet> packet, const Address &from, const Address &to, Time sendTime)
{
  packetsReceived++;
  totalDelay += Simulator::Now ().GetSeconds () - sendTime.GetSeconds ();
}

class UdpAppWithTimestamp : public Application
{
public:
  UdpAppWithTimestamp () {}
  virtual ~UdpAppWithTimestamp () {}
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_pktInterval = pktInterval;
    m_packetsSent = 0;
  }
private:
  virtual void StartApplication ()
  {
    m_socket->Bind ();
    m_socket->Connect (m_peer);
    SendPacket ();
  }
  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }
  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    packet->AddPacketTag (TimestampTag (Simulator::Now ().GetNanoSeconds ()));
    m_socket->Send (packet);
    packetsSent++;
    m_packetsSent++;
    if (m_packetsSent < m_nPackets)
      {
        Simulator::Schedule (m_pktInterval, &UdpAppWithTimestamp::SendPacket, this);
      }
  }
  class TimestampTag : public Tag
  {
  public:
    TimestampTag () : m_timestamp (0) {}
    TimestampTag (uint64_t ts) : m_timestamp (ts) {}
    static TypeId GetTypeId (void)
    {
      static TypeId tid = TypeId ("TimestampTag")
        .SetParent<Tag> ()
        .AddConstructor<TimestampTag> ();
      return tid;
    }
    virtual TypeId GetInstanceTypeId (void) const { return GetTypeId (); }
    virtual uint32_t GetSerializedSize (void) const { return sizeof (uint64_t); }
    virtual void Serialize (TagBuffer i) const { i.WriteU64 (m_timestamp); }
    virtual void Deserialize (TagBuffer i) { m_timestamp = i.ReadU64 (); }
    virtual void Print (std::ostream &os) const { os << "t=" << m_timestamp; }
    void SetTimestamp (uint64_t ts) { m_timestamp = ts; }
    uint64_t GetTimestamp () const { return m_timestamp; }
  private:
    uint64_t m_timestamp;
  };
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_pktInterval;
  uint32_t m_packetsSent;
};

void ReceivePacketWithLatency (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      UdpAppWithTimestamp::TimestampTag tag;
      if (packet->PeekPacketTag (tag))
        {
          double delay = (Simulator::Now ().GetNanoSeconds () - tag.GetTimestamp ()) / 1e9;
          totalDelay += delay;
        }
      packetsReceived++;
    }
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 3;
  double simTime = 30.0;
  double distance = 100.0;
  uint32_t packetSize = 512;
  uint32_t numPackets = 100;
  double interval = 0.5; // seconds
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                               "DataMode", StringValue ("DsssRate11Mbps"),
                               "ControlMode", StringValue ("DsssRate11Mbps"));
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, distance * 3, 0, distance * 3)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=8.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (20.0),
                                "MinY", DoubleValue (20.0),
                                "DeltaX", DoubleValue (distance),
                                "DeltaY", DoubleValue (distance),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  // Set up UDP receiver on node 2
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (2), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9000);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacketWithLatency));

  // Set up UDP sender on node 0 to node 2
  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), tid);
  Address remoteAddr (InetSocketAddress (interfaces.GetAddress (2), 9000));
  Ptr<UdpAppWithTimestamp> app = CreateObject<UdpAppWithTimestamp> ();
  app->Setup (source, remoteAddr, packetSize, numPackets, Seconds (interval));
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (2.0));
  app->SetStopTime (Seconds (simTime - 1));
  
  // NetAnim
  AnimationInterface anim ("manet3netanim.xml");
  for (uint32_t i=0; i<numNodes; ++i)
    anim.UpdateNodeDescription (nodes.Get (i), "N" + std::to_string (i));
  anim.EnablePacketMetadata (true);

  // Enable traces/logs
  wifiPhy.EnablePcapAll ("manet3");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  double pdr = packetsSent > 0 ? (static_cast<double> (packetsReceived) / packetsSent) * 100.0 : 0.0;
  double avgDelay = packetsReceived > 0 ? totalDelay / packetsReceived : 0.0;

  std::cout << "Packets Sent: " << packetsSent << std::endl;
  std::cout << "Packets Received: " << packetsReceived << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
  std::cout << "Average End-to-End Latency: " << avgDelay << " seconds" << std::endl;

  Simulator::Destroy ();
  return 0;
}