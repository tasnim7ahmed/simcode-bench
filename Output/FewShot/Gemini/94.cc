#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-extension-header.h"
#include <iostream>

using namespace ns3;

class UdpClient6 : public Application
{
public:
  UdpClient6 ();
  virtual ~UdpClient6 ();

  void Setup (Address address, uint32_t packetSize, uint32_t maxPackets, DataRate dataRate, Time interPacketInterval, uint8_t tclassValue, uint8_t hopLimitValue);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void ReceivedResponse (Ptr<Socket> socket);
  void SocketErr (Ptr<Socket> socket);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_maxPackets;
  DataRate m_dataRate;
  Time m_interPacketInterval;
  Ptr<Socket> m_socket;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  uint8_t m_tclassValue;
  uint8_t m_hopLimitValue;
};

UdpClient6::UdpClient6 ()
  : m_peerAddress (),
    m_packetSize (0),
    m_maxPackets (0),
    m_dataRate (0),
    m_interPacketInterval (0),
    m_socket (nullptr),
    m_packetsSent (0),
    m_tclassValue (0),
    m_hopLimitValue (0)
{
}

UdpClient6::~UdpClient6 ()
{
  if (m_socket != nullptr)
  {
    m_socket->Close ();
  }
}

void
UdpClient6::Setup (Address address, uint32_t packetSize, uint32_t maxPackets, DataRate dataRate, Time interPacketInterval, uint8_t tclassValue, uint8_t hopLimitValue)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_maxPackets = maxPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = interPacketInterval;
  m_tclassValue = tclassValue;
  m_hopLimitValue = hopLimitValue;
}

void
UdpClient6::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory6"));

  if (m_tclassValue != 0)
  {
    m_socket->SetTtl (m_tclassValue);
  }

  if (m_hopLimitValue != 0)
  {
    m_socket->SetHopLimit (m_hopLimitValue);
  }

  m_socket->Bind ();
  m_socket->Connect (m_peerAddress);
  m_socket->SetRecvCallback (MakeCallback (&UdpClient6::ReceivedResponse, this));
  m_socket->SetErrCallback (MakeCallback (&UdpClient6::SocketErr, this));
  m_packetsSent = 0;
  SendPacket ();
}

void
UdpClient6::StopApplication (void)
{
  Simulator::Cancel (m_sendEvent);
  if (m_socket != nullptr)
  {
    m_socket->Close ();
  }
}

void
UdpClient6::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_maxPackets)
  {
    m_sendEvent = Simulator::Schedule (m_interPacketInterval, &UdpClient6::SendPacket, this);
  }
}

void
UdpClient6::ReceivedResponse (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
}

void
UdpClient6::SocketErr (Ptr<Socket> socket)
{
  NS_LOG_INFO ("Socket error!");
}

class UdpServer6 : public Application
{
public:
  UdpServer6 ();
  virtual ~UdpServer6 ();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);
  void AcceptConnection (Ptr<Socket> socket, const Address& from);
  void ConnectionClosed (Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  std::list<Ptr<Socket>> m_socketList;
};

UdpServer6::UdpServer6 ()
  : m_port (0),
    m_socket (nullptr),
    m_socketList ()
{
}

UdpServer6::~UdpServer6 ()
{
  if (m_socket != nullptr)
  {
    m_socket->Close ();
  }
  for (auto socket : m_socketList)
  {
    socket->Close ();
  }
  m_socketList.clear ();
}

void
UdpServer6::Setup (uint16_t port)
{
  m_port = port;
}

void
UdpServer6::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory6"));
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&UdpServer6::HandleRead, this));
  m_socket->SetAcceptCallback (MakeCallback (&UdpServer6::AcceptConnection, this));
  m_socket->SetCloseCallback (MakeCallback (&UdpServer6::ConnectionClosed, this));
  m_socket->Listen ();
  m_socket->SetAttribute ("IpRecvTclass", BooleanValue (true));
  m_socket->SetAttribute ("IpRecvHopLimit", BooleanValue (true));
}

void
UdpServer6::StopApplication (void)
{
  if (m_socket != nullptr)
  {
    m_socket->Close ();
  }
  for (auto socket : m_socketList)
  {
    socket->Close ();
  }
  m_socketList.clear ();
}

void
UdpServer6::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);

  Ipv6Address origin;
  uint32_t hopLimit;
  uint32_t trafficClass;

  Ptr<Ipv6> ipv6 = GetNode()->GetObject<Ipv6>();

  if (ipv6 != nullptr) {
    auto interfaceIndex = socket->GetBoundNetDevice()->GetIfIndex();
    auto interfaceIpv6 = ipv6->GetAddress(interfaceIndex, 1).GetAddress().ConvertTo<Ipv6Address>();
    origin = interfaceIpv6;
  }
  socket->GetSockOpt ("IpTclass", trafficClass);
  socket->GetSockOpt ("IpHopLimit", hopLimit);

  std::cout << Simulator::Now ().Seconds () << " Server received " << packet->GetSize ()
            << " bytes from " << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
            << " with TCLASS " << trafficClass
            << " and HOPLIMIT " << hopLimit << std::endl;
}


void
UdpServer6::AcceptConnection (Ptr<Socket> socket, const Address& from)
{
  std::cout << Simulator::Now ().Seconds () << " Server accepted connection from " << from << std::endl;
  Ptr<Socket> newSocket = socket->Accept ();
  m_socketList.push_back (newSocket);
}

void
UdpServer6::ConnectionClosed (Ptr<Socket> socket)
{
  for (auto i = m_socketList.begin (); i != m_socketList.end (); ++i)
  {
    if (*i == socket)
    {
      m_socketList.erase (i);
      break;
    }
  }
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 1;
  double interval = 1.0;
  uint8_t tclassValue = 0;
  uint8_t hopLimitValue = 0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if all packets are received", verbose);
  cmd.AddValue ("packetSize", "Size of the packets to send", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets", interval);
  cmd.AddValue ("tclass", "TCLASS value to set on the socket", tclassValue);
  cmd.AddValue ("hoplimit", "HOPLIMIT value to set on the socket", hopLimitValue);
  cmd.Parse (argc, argv);

  if (verbose)
  {
    LogComponentEnable ("UdpClient6", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer6", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  interfaces.GetAddress (0, 1);
  interfaces.GetAddress (1, 1);

  interfaces.SetForwarding (0, true);
  interfaces.SetForwarding (1, true);

  interfaces.SetDefaultRouteInAllNodes (0);

  uint16_t port = 9;

  UdpServer6Helper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClient6Helper client (interfaces.GetAddress (1, 1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  Ptr<UdpClient6> udpClient = DynamicCast<UdpClient6>(client.GetClient());
  udpClient->Setup (Inet6SocketAddress (interfaces.GetAddress (1, 1), port), packetSize, numPackets, DataRate ("5Mbps"), Seconds (interval), tclassValue, hopLimitValue);
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));


  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}