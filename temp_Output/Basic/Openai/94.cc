#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6UdpSocketOptionsExample");

// Receiver application to log TCLASS and HOPLIMIT
class Udp6Receiver : public Application
{
public:
  Udp6Receiver () : m_socket (0) {}
  virtual ~Udp6Receiver () { m_socket = 0; }

  void Setup (Address bindAddress, uint16_t port)
  {
    m_bindAddress = bindAddress;
    m_port = port;
  }

  void SetRecvTclass (bool enable) { m_recvTclass = enable; }
  void SetRecvHoplimit (bool enable) { m_recvHoplimit = enable; }

protected:
  virtual void StartApplication (void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
        Inet6SocketAddress localAddress = Inet6SocketAddress (Ipv6Address::ConvertFrom(m_bindAddress), m_port);
        m_socket->Bind (localAddress);
        m_socket->SetRecvCallback (MakeCallback (&Udp6Receiver::HandleRead, this));

        // Set recv options if requested
        if (m_recvTclass)
          m_socket->SetRecvPktInfo (true); // required to get ancillary data
        if (m_recvHoplimit)
          m_socket->SetRecvHopLimit (true);
        if (m_recvTclass)
          m_socket->SetRecvTClass (true);
      }
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address address;
    struct
    {
      int32_t tclass;
      int32_t hoplimit;
      bool hasTclass;
      bool hasHopLimit;
    } ipv6Anc;
    ipv6Anc.tclass = -1;
    ipv6Anc.hoplimit = -1;
    ipv6Anc.hasTclass = false;
    ipv6Anc.hasHopLimit = false;

    while (Ptr<Packet> packet = socket->RecvFrom (address))
      {
        SocketIpv6RecvInfoTag ipv6Info;
        if (packet->PeekPacketTag(ipv6Info))
          {
            ipv6Anc.tclass = ipv6Info.GetTclass ();
            ipv6Anc.hasTclass = true;
            ipv6Anc.hoplimit = ipv6Info.GetHopLimit ();
            ipv6Anc.hasHopLimit = true;
          }

        if (ipv6Anc.hasTclass)
          {
            std::cout << "At " << Simulator::Now ().GetSeconds ()
                      << "s, n1 received " << packet->GetSize ()
                      << " bytes; TCLASS=" << ipv6Anc.tclass
                      << "; ";
          }
        else
          {
            std::cout << "At " << Simulator::Now ().GetSeconds ()
                      << "s, n1 received " << packet->GetSize ()
                      << " bytes; TCLASS=not available; ";
          }
        if (ipv6Anc.hasHopLimit)
          {
            std::cout << "HOPLIMIT=" << ipv6Anc.hoplimit << std::endl;
          }
        else
          {
            std::cout << "HOPLIMIT=not available" << std::endl;
          }
      }
  }

private:
  Ptr<Socket> m_socket;
  Address m_bindAddress;
  uint16_t m_port;
  bool m_recvTclass = false;
  bool m_recvHoplimit = false;
};


// Sender application with configurable TCLASS and HOPLIMIT
class Udp6Sender : public Application
{
public:
  Udp6Sender () : m_socket (0) {}
  virtual ~Udp6Sender () { m_socket = 0; }

  void Setup (Address address, uint16_t port,
              uint32_t packetSize, uint32_t totalPackets, Time interval,
              int32_t tclass, int32_t hoplimit)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSize = packetSize;
    m_nPackets = totalPackets;
    m_interval = interval;
    m_tclass = tclass;
    m_hoplimit = hoplimit;
  }

protected:
  virtual void StartApplication (void)
  {
    m_sent = 0;
    m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
    m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));

    // Set TCLASS (traffic class) and hoplimit socket options if requested
    if (m_tclass >= 0)
      {
        m_socket->SetIpTClass (m_tclass);
      }
    if (m_hoplimit > 0)
      {
        m_socket->SetIpHopLimit (m_hoplimit);
      }
    SendPacket ();
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);
    ++m_sent;
    if (m_sent < m_nPackets)
      {
        Simulator::Schedule (m_interval, &Udp6Sender::SendPacket, this);
      }
  }

private:
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  int32_t m_tclass = -1;
  int32_t m_hoplimit = -1;
  uint32_t m_sent = 0;
};


int main (int argc, char *argv[])
{
  // Default parameter values
  uint32_t packetSize = 512;
  uint32_t nPackets = 10;
  double interval = 1.0;
  int32_t tclass = -1;
  int32_t hoplimit = -1;
  bool recvTclass = true;
  bool recvHoplimit = true;

  CommandLine cmd;
  cmd.AddValue ("packetSize",   "Size of each UDP packet in bytes", packetSize);
  cmd.AddValue ("nPackets",     "Number of UDP packets to send", nPackets);
  cmd.AddValue ("interval",     "Time between packets (s)", interval);
  cmd.AddValue ("tclass",       "IPv6 Traffic Class (TCLASS) socket option (-1 for default)", tclass);
  cmd.AddValue ("hoplimit",     "IPv6 Hop Limit socket option (-1 for default)", hoplimit);
  cmd.AddValue ("recvTclass",   "Enable RECVTCLASS option on receiver", recvTclass);
  cmd.AddValue ("recvHoplimit", "Enable RECVHOPLIMIT option on receiver", recvHoplimit);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase ("2001:0db8::", Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);
  interfaces.SetForwarding (0, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  uint16_t udpPort = 50000;

  Ptr<Udp6Receiver> receiverApp = CreateObject<Udp6Receiver> ();
  receiverApp->Setup (interfaces.GetAddress (1, 1), udpPort);
  receiverApp->SetRecvTclass (recvTclass);
  receiverApp->SetRecvHoplimit (recvHoplimit);
  nodes.Get(1)->AddApplication (receiverApp);
  receiverApp->SetStartTime (Seconds (0.0));
  receiverApp->SetStopTime (Seconds (nPackets * interval + 5.0));

  Ptr<Udp6Sender> senderApp = CreateObject<Udp6Sender> ();
  senderApp->Setup (interfaces.GetAddress (1,1), udpPort,
                    packetSize, nPackets,
                    Seconds (interval), tclass, hoplimit);
  nodes.Get(0)->AddApplication (senderApp);
  senderApp->SetStartTime (Seconds (1.0));
  senderApp->SetStopTime (Seconds (nPackets * interval + 2.0));

  Simulator::Stop (Seconds (nPackets * interval + 6.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}