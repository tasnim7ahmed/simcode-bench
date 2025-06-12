#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpClientServer");

class UdpClient : public Application
{
public:
  UdpClient ();
  virtual ~UdpClient ();

  void Setup (Address address1, Address address2, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void ScheduleTx (void);
  void AckReceived (Ptr<Socket> socket);

  Address m_peerAddress1;
  Address m_peerAddress2;
  uint32_t  m_packetSize;
  uint32_t  m_numPackets;
  DataRate  m_dataRate;
  uint32_t  m_packetsSent;
  bool m_running;
  Ptr<Socket> m_socket1;
  Ptr<Socket> m_socket2;
  EventId   m_sendEvent;
};

UdpClient::UdpClient ()
  : m_peerAddress1 (),
    m_peerAddress2 (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_packetsSent (0),
    m_running (false),
    m_socket1 (nullptr),
    m_socket2 (nullptr),
    m_sendEvent ()
{
}

UdpClient::~UdpClient ()
{
  if (m_socket1)
    {
      m_socket1->Close ();
    }
  if (m_socket2)
    {
      m_socket2->Close ();
    }
}

void
UdpClient::Setup (Address address1, Address address2, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress1 = address1;
  m_peerAddress2 = address2;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void
UdpClient::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket1 = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket2 = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

  m_socket1->Bind ();
  m_socket2->Bind ();

  m_socket1->Connect (m_peerAddress1);
  m_socket2->Connect (m_peerAddress2);

  ScheduleTx ();
}

void
UdpClient::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket1)
    {
      m_socket1->Close ();
    }
  if (m_socket2)
    {
      m_socket2->Close ();
    }
}

void
UdpClient::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket1->Send (packet);
  m_socket2->Send (packet->Copy());

  if (++m_packetsSent < m_numPackets)
    {
      ScheduleTx ();
    }
}

void
UdpClient::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
      m_sendEvent = Simulator::Schedule (tNext, &UdpClient::SendPacket, this);
    }
}

class UdpServer : public Application
{
public:
  UdpServer ();
  virtual ~UdpServer ();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);
  void Bind ();

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
};

UdpServer::UdpServer ()
  : m_port (0),
    m_socket (nullptr),
    m_running (false)
{
}

UdpServer::~UdpServer ()
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpServer::Setup (uint16_t port)
{
  m_port = port;
}

void
UdpServer::StartApplication (void)
{
  m_running = true;
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
}

void
UdpServer::StopApplication (void)
{
  m_running = false;

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpServer::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_INFO ("Received " << packet->GetSize () << " bytes from " << from);
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClientServer", LOG_LEVEL_INFO);

  NodeContainer p2pNodes;
  p2pNodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices01 = pointToPoint.Install (p2pNodes.Get (0), p2pNodes.Get (1));
  NetDeviceContainer p2pDevices02 = pointToPoint.Install (p2pNodes.Get (0), p2pNodes.Get (2));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices = csma.Install (p2pNodes.Get (0));

  InternetStackHelper stack;
  stack.Install (p2pNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces01 = address.Assign (p2pDevices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02 = address.Assign (p2pDevices02);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port1 = 9;
  uint16_t port2 = 10;

  UdpServerHelper server1 (port1);
  ApplicationContainer serverApps1 = server1.Install (p2pNodes.Get (1));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (10.0));

  UdpServerHelper server2 (port2);
  ApplicationContainer serverApps2 = server2.Install (p2pNodes.Get (2));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));

  UdpClientHelper client (p2pInterfaces01.GetAddress (1), port1);
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("NumPackets", UintegerValue (100));
  UdpClientHelper client2 (p2pInterfaces02.GetAddress (1), port2);
  client2.SetAttribute ("PacketSize", UintegerValue (1024));
  client2.SetAttribute ("NumPackets", UintegerValue (100));

  ApplicationContainer clientApps = client.Install (p2pNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}