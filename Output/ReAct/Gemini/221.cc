#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CustomUdp");

class UdpClient : public Application
{
public:
  UdpClient ();
  virtual ~UdpClient();

  void Setup (Address address1, Address address2, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);

  Address m_remoteAddress1;
  Address m_remoteAddress2;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  uint32_t m_packetsSent;
  bool m_running;
  Time m_interPacketInterval;
  SocketPtr m_socket1;
  SocketPtr m_socket2;
  EventId m_sendEvent;
};

UdpClient::UdpClient ()
  : m_remoteAddress1 (),
    m_remoteAddress2 (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_packetsSent (0),
    m_running (false),
    m_interPacketInterval ()
{
}

UdpClient::~UdpClient()
{
  m_socket1 = nullptr;
  m_socket2 = nullptr;
}

void
UdpClient::Setup (Address address1, Address address2, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_remoteAddress1 = address1;
  m_remoteAddress2 = address2;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = Time (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
}

void
UdpClient::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket1 = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket1->Bind ();
  m_socket1->Connect (m_remoteAddress1);
  m_socket1->SetAllowBroadcast (true);

  m_socket2 = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket2->Bind ();
  m_socket2->Connect (m_remoteAddress2);
  m_socket2->SetAllowBroadcast (true);

  SendPacket ();
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
      m_sendEvent = Simulator::Schedule (m_interPacketInterval, &UdpClient::SendPacket, this);
    }
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
  void CompleteReceive (Ptr<Socket> socket);
  void ErrTrace (Ptr<Socket> socket);

  uint16_t m_port;
  SocketPtr m_socket;
  bool m_running;
};

UdpServer::UdpServer ()
  : m_port (0),
    m_socket (0),
    m_running (false)
{
}

UdpServer::~UdpServer()
{
    m_socket = nullptr;
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

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
  m_socket->SetCloseCallbacks (MakeCallback (&UdpServer::CompleteReceive, this),MakeCallback (&UdpServer::CompleteReceive, this));
  m_socket->SetErrCallback (MakeCallback (&UdpServer::ErrTrace, this));

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
      NS_LOG_INFO ("Received one packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort() << " at time " << Simulator::Now ().GetSeconds ());
    }
}

void
UdpServer::CompleteReceive (Ptr<Socket> socket)
{
  NS_LOG_INFO ("CompleteReceive called ");
}

void
UdpServer::ErrTrace (Ptr<Socket> socket)
{
  NS_LOG_INFO ("UdpServer::ErrTrace called ");
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("CustomUdp", LOG_LEVEL_INFO);

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  DataRate dataRate ("1Mbps");
  uint16_t port1 = 9;
  uint16_t port2 = 10;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("dataRate", "Sending rate", dataRate);
  cmd.AddValue ("port1", "Port number for server1", port1);
  cmd.AddValue ("port2", "Port number for server2", port2);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer clientNode;
  clientNode.Create (1);
  NodeContainer serverNodes;
  serverNodes.Create (2);
  NodeContainer switchNode;
  switchNode.Create(1);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer clientSwitchDevices = csma.Install (NodeContainer (clientNode.Get (0), switchNode.Get (0)));
  NetDeviceContainer server1SwitchDevices = csma.Install (NodeContainer (serverNodes.Get (0), switchNode.Get (0)));
  NetDeviceContainer server2SwitchDevices = csma.Install (NodeContainer (serverNodes.Get (1), switchNode.Get (0)));

  InternetStackHelper stack;
  stack.Install (clientNode);
  stack.Install (serverNodes);
  stack.Install(switchNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientSwitchInterfaces = address.Assign (clientSwitchDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer server1SwitchInterfaces = address.Assign (server1SwitchDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer server2SwitchInterfaces = address.Assign (server2SwitchDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpClient clientApp;
  clientApp.Setup (InetSocketAddress (server1SwitchInterfaces.GetAddress (0), port1), InetSocketAddress (server2SwitchInterfaces.GetAddress (0), port2), packetSize, numPackets, dataRate);
  clientApp.SetStartTime (Seconds (1.0));
  clientApp.SetStopTime (Seconds (simulationTime));
  clientApp.SetNode (clientNode.Get (0));
  clientApp.Start ();

  UdpServer serverApp1;
  serverApp1.Setup (port1);
  serverApp1.SetStartTime (Seconds (0.0));
  serverApp1.SetStopTime (Seconds (simulationTime + 1));
  serverApp1.SetNode (serverNodes.Get (0));
  serverApp1.Start ();

  UdpServer serverApp2;
  serverApp2.Setup (port2);
  serverApp2.SetStartTime (Seconds (0.0));
  serverApp2.SetStopTime (Seconds (simulationTime + 1));
  serverApp2.SetNode (serverNodes.Get (1));
  serverApp2.Start ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}