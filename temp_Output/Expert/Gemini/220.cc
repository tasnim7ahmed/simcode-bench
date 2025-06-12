#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

class UdpClient : public Application
{
public:
  UdpClient ();
  virtual ~UdpClient();

  void Setup (Address address, uint32_t packetSize, uint32_t maxPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void PacketReceived (Ptr<Socket> socket);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_maxPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  Time m_interPacketInterval;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient () :
  m_peerAddress (),
  m_packetSize (0),
  m_maxPackets (0),
  m_dataRate (0),
  m_socket (nullptr),
  m_interPacketInterval (),
  m_sendEvent (),
  m_packetsSent (0)
{
}

UdpClient::~UdpClient()
{
  if (m_socket) {
    m_socket->Close ();
  }
}

void
UdpClient::Setup (Address address, uint32_t packetSize, uint32_t maxPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_maxPackets = maxPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = Time (Seconds (8 * (double) m_packetSize / (double) m_dataRate.GetBitRate ()));
}

void
UdpClient::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->Bind ();
  m_socket->Connect (m_peerAddress);
  m_socket->SetRecvCallback (MakeCallback (&UdpClient::PacketReceived, this));

  m_packetsSent = 0;
  SendPacket ();
}

void
UdpClient::StopApplication (void)
{
  Simulator::Cancel (m_sendEvent);
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpClient::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_maxPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interPacketInterval, &UdpClient::SendPacket, this);
    }
}

void
UdpClient::PacketReceived (Ptr<Socket> socket)
{

}

class UdpServer : public Application
{
public:
  UdpServer ();
  virtual ~UdpServer();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);
  void PrintContents (Ptr<Socket> socket, Ptr<Packet> packet, const Address &from);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  Address m_localAddress;
};

UdpServer::UdpServer () : m_port (0), m_socket (nullptr), m_localAddress()
{
}

UdpServer::~UdpServer()
{
  if (m_socket) {
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
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_localAddress = local;
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
}

void
UdpServer::StopApplication (void)
{
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
      PrintContents (socket, packet, from);
    }
}

void
UdpServer::PrintContents (Ptr<Socket> socket, Ptr<Packet> packet, const Address &from)
{
  std::cout << Simulator::Now ().AsDouble () << " Received one packet from " << from << " of size " << packet->GetSize () << std::endl;
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;

  UdpServerHelper serverHelper (port);
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (1));
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps.Get(0));
  server->Setup(port);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper clientHelper (interfaces.GetAddress (1), port);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  Ptr<UdpClient> client = DynamicCast<UdpClient> (clientApps.Get(0));
  client->Setup(interfaces.GetAddress (1), 1024, 1000, DataRate ("1Mbps"));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}