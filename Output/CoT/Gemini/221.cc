#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CustomUdp");

class CustomUdpClient : public Application
{
public:
  CustomUdpClient ();
  virtual ~CustomUdpClient ();

  void Setup (Address address1, Address address2, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void ScheduleTx (void);

  Address m_address1;
  Address m_address2;
  uint32_t  m_packetSize;
  uint32_t  m_numPackets;
  DataRate  m_dataRate;
  Socket    m_socket1;
  Socket    m_socket2;
  EventId   m_sendEvent;
  bool      m_running;
  uint32_t  m_packetsSent;
};

CustomUdpClient::CustomUdpClient ()
  : m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_socket1 (nullptr),
    m_socket2 (nullptr),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

CustomUdpClient::~CustomUdpClient ()
{
  m_socket1.reset();
  m_socket2.reset();
}

void
CustomUdpClient::Setup (Address address1, Address address2, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_address1 = address1;
  m_address2 = address2;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void
CustomUdpClient::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket1 = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket2 = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket1->Bind ();
  m_socket2->Bind ();
  m_socket1->Connect (m_address1);
  m_socket2->Connect (m_address2);

  SendPacket ();
}

void
CustomUdpClient::StopApplication (void)
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
CustomUdpClient::SendPacket (void)
{
  if (m_packetsSent < m_numPackets)
    {
      Ptr<Packet> packet = Create<Packet> (m_packetSize);
      m_socket1->Send (packet);
      m_socket2->Send (packet->Copy());
      m_packetsSent++;

      ScheduleTx ();
    }
}

void
CustomUdpClient::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
      m_sendEvent = Simulator::Schedule (tNext, &CustomUdpClient::SendPacket, this);
    }
}


class CustomUdpServer : public Application
{
public:
  CustomUdpServer ();
  virtual ~CustomUdpServer();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);
  void ReadData (Ptr<Socket> socket);

  uint16_t m_port;
  Socket m_socket;
  bool m_running;
};

CustomUdpServer::CustomUdpServer() : m_port(0), m_socket(nullptr), m_running(false) {}
CustomUdpServer::~CustomUdpServer() { m_socket.reset(); }

void CustomUdpServer::Setup(uint16_t port) { m_port = port; }

void CustomUdpServer::StartApplication() {
  m_running = true;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
}

void CustomUdpServer::StopApplication() {
  m_running = false;
  if (m_socket) {
    m_socket->Close();
  }
}

void CustomUdpServer::HandleRead(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
  if (packet != nullptr) {
    NS_LOG_INFO ("Received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort());
  }
}

int main (int argc, char *argv[])
{
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetLevel("CustomUdp", LOG_LEVEL_ALL);

  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  DataRate dataRate ("1Mbps");
  uint16_t port1 = 9;
  uint16_t port2 = 10;

  NodeContainer clientNode;
  clientNode.Create (1);
  NodeContainer serverNodes;
  serverNodes.Create (2);
  NodeContainer switchNode;
  switchNode.Create(1);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (dataRate));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer clientSwitchDevices = csma.Install (NodeContainer (clientNode.Get(0), switchNode.Get(0)));
  NetDeviceContainer switchServer1Devices = csma.Install (NodeContainer (switchNode.Get(0), serverNodes.Get(0)));
  NetDeviceContainer switchServer2Devices = csma.Install (NodeContainer (switchNode.Get(0), serverNodes.Get(1)));

  InternetStackHelper internet;
  internet.Install (clientNode);
  internet.Install (serverNodes);
  internet.Install(switchNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientSwitchIfaces = ipv4.Assign (clientSwitchDevices);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer switchServer1Ifaces = ipv4.Assign (switchServer1Devices);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer switchServer2Ifaces = ipv4.Assign (switchServer2Devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Node> client = clientNode.Get (0);
  Ptr<Node> server1 = serverNodes.Get (0);
  Ptr<Node> server2 = serverNodes.Get (1);

  CustomUdpServer serverApp1;
  serverApp1.Setup(port1);
  server1->AddApplication(serverApp1);
  serverApp1.SetStartTime(Seconds(0.0));
  serverApp1.SetStopTime(Seconds(10.0));

  CustomUdpServer serverApp2;
  serverApp2.Setup(port2);
  server2->AddApplication(serverApp2);
  serverApp2.SetStartTime(Seconds(0.0));
  serverApp2.SetStopTime(Seconds(10.0));

  Address serverAddress1 (InetSocketAddress (switchServer1Ifaces.GetAddress (1), port1));
  Address serverAddress2 (InetSocketAddress (switchServer2Ifaces.GetAddress (1), port2));

  CustomUdpClient clientApp;
  clientApp.Setup (serverAddress1, serverAddress2, packetSize, numPackets, dataRate);
  client->AddApplication (clientApp);
  clientApp.SetStartTime (Seconds (1.0));
  clientApp.SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}