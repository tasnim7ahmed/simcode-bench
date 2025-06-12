#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class Udp6CustomReceiver : public Application
{
public:
  Udp6CustomReceiver () : m_socket (0) {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

  void SetRecvTclass (bool enable) { m_recvTclass = enable; }
  void SetRecvHopLimit (bool enable) { m_recvHopLimit = enable; }

private:
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
    Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
    if (m_socket->Bind (local) == -1)
      {
        NS_FATAL_ERROR ("Unable to bind socket");
      }
    if (m_recvTclass)
      {
        m_socket->SetRecvPktInfo6 (true);
        m_socket->SetRecvTrafficClass (true);
      }
    if (m_recvHopLimit)
      {
        m_socket->SetRecvHopLimit (true);
      }
    m_socket->SetRecvCallback (MakeCallback (&Udp6CustomReceiver::HandleRead, this));
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom (from))
      {
        SocketIpv6HopLimitTag hopLimitTag;
        SocketIpv6TrafficClassTag tclassTag;
        uint8_t hoplimit = 0xff;
        uint8_t tclass = 0xff;
        if (packet->PeekPacketTag (hopLimitTag))
          {
            hoplimit = hopLimitTag.GetHopLimit ();
          }
        if (packet->PeekPacketTag (tclassTag))
          {
            tclass = tclassTag.GetTrafficClass ();
          }
        NS_LOG_UNCOND ("At " << Simulator::Now ().GetSeconds () <<
              "s pkt recv at " << Inet6SocketAddress::ConvertFrom (from).GetIpv6 () <<
              " TCLASS=" << static_cast<uint32_t>(tclass) <<
              " HOPLIMIT=" << static_cast<uint32_t>(hoplimit) <<
              " size=" << packet->GetSize ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
  bool m_recvTclass = false;
  bool m_recvHopLimit = false;
};

class Udp6CustomSender : public Application
{
public:
  Udp6CustomSender () : m_socket (0), m_sendEvent (), m_sent (0) {}

  void Setup (Ipv6Address dstAddress, uint16_t dstPort,
              uint32_t pktSize, uint32_t nPackets, Time interval,
              int32_t tclass, int32_t hoplimit)
  {
    m_dstAddress = dstAddress;
    m_dstPort = dstPort;
    m_pktSize = pktSize;
    m_nPackets = nPackets;
    m_interval = interval;
    m_tclass = tclass;
    m_hoplimit = hoplimit;
  }

private:
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
    m_socket->Bind ();
    if (m_tclass >= 0)
      {
        m_socket->SetIp6Tclass (m_tclass);
      }
    if (m_hoplimit >= 0)
      {
        m_socket->SetIp6HopLimit (m_hoplimit);
      }
    m_peer = Inet6SocketAddress (m_dstAddress, m_dstPort);
    SendPacket ();
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
    Simulator::Cancel (m_sendEvent);
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_pktSize);
    m_socket->SendTo (packet, 0, m_peer);
    ++m_sent;
    if (m_sent < m_nPackets)
      {
        m_sendEvent = Simulator::Schedule (m_interval, &Udp6CustomSender::SendPacket, this);
      }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  Ipv6Address m_dstAddress;
  uint16_t m_dstPort;
  uint32_t m_pktSize;
  uint32_t m_nPackets;
  Time m_interval;
  int32_t m_tclass;
  int32_t m_hoplimit;
  uint32_t m_sent;
  EventId m_sendEvent;
};

int main (int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 10;
  double interval = 1.0;
  int32_t tclass = -1;
  int32_t hoplimit = -1;
  bool recvTclassOption = true;
  bool recvHopLimitOption = true;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of each packet (bytes)", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("tclass", "IPv6 Traffic Class value for sender socket (0-255, -1=unset)", tclass);
  cmd.AddValue ("hoplimit", "IPv6 Hop Limit for sender socket (0-255, -1=unset)", hoplimit);
  cmd.AddValue ("recvTclass", "Receiver: enable RECVTCLASS socket option", recvTclassOption);
  cmd.AddValue ("recvHoplimit", "Receiver: enable RECVHOPLIMIT socket option", recvHopLimitOption);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifaces = ipv6.Assign (devices);
  ifaces.SetForwarding (0, true);
  ifaces.SetDefaultRouteInAllNodes (0);

  uint16_t port = 9999;

  Ptr<Udp6CustomReceiver> server = CreateObject<Udp6CustomReceiver> ();
  server->Setup (port);
  server->SetRecvTclass (recvTclassOption);
  server->SetRecvHopLimit (recvHopLimitOption);
  nodes.Get (1)->AddApplication (server);
  server->SetStartTime (Seconds (0.0));
  server->SetStopTime (Seconds (100.0));

  Ptr<Udp6CustomSender> client = CreateObject<Udp6CustomSender> ();
  client->Setup (ifaces.GetAddress (1,1), port, packetSize, numPackets, Seconds (interval), tclass, hoplimit);
  nodes.Get (0)->AddApplication (client);
  client->SetStartTime (Seconds (1.0));
  client->SetStopTime (Seconds (100.0));

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}