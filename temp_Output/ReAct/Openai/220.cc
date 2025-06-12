#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpSocketExample");

class UdpClientApp : public Application
{
public:
  UdpClientApp();
  virtual ~UdpClientApp();

  void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTx();
  void SendPacket();

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  uint32_t        m_sent;
  EventId         m_sendEvent;
  bool            m_running;
};

UdpClientApp::UdpClientApp()
  : m_socket(0),
    m_peer(),
    m_packetSize(0),
    m_nPackets(0),
    m_pktInterval(),
    m_sent(0),
    m_sendEvent(),
    m_running(false)
{}

UdpClientApp::~UdpClientApp()
{
  m_socket = 0;
}

void
UdpClientApp::Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
}

void
UdpClientApp::StartApplication(void)
{
  m_running = true;
  m_sent = 0;
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Connect(m_peer);
    }
  SendPacket();
}

void
UdpClientApp::StopApplication(void)
{
  m_running = false;
  if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }
  if (m_socket)
    {
      m_socket->Close();
    }
}

void
UdpClientApp::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  ++m_sent;
  if (m_sent < m_nPackets)
    {
      ScheduleTx();
    }
}

void
UdpClientApp::ScheduleTx()
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule(m_pktInterval, &UdpClientApp::SendPacket, this);
    }
}

// Server

class UdpServerApp : public Application
{
public:
  UdpServerApp();
  virtual ~UdpServerApp();

  void Setup(Address address);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  Address         m_local;
};

UdpServerApp::UdpServerApp()
  : m_socket(0),
    m_local()
{}

UdpServerApp::~UdpServerApp()
{
  m_socket = 0;
}

void
UdpServerApp::Setup(Address address)
{
  m_local = address;
}

void
UdpServerApp::StartApplication(void)
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress::ConvertFrom(m_local);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
    }
}

void
UdpServerApp::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
    }
}

void
UdpServerApp::HandleRead(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      if (packet->GetSize() > 0)
        {
          NS_LOG_UNCOND("At time " << Simulator::Now().GetSeconds()
                          << "s server received " << packet->GetSize()
                          << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("CustomUdpSocketExample", LOG_LEVEL_INFO);

  // Nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Point-to-point channel
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = p2p.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP
  Ipv4AddressHelper addressing;
  addressing.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = addressing.Assign(devices);

  uint16_t serverPort = 4000;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), serverPort));

  // Install server on node 1
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
  serverApp->Setup(serverAddress);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  // Install client on node 0
  uint32_t packetSize = 1024;
  uint32_t nPackets = 100;
  Time pktInterval = Seconds(0.1);

  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
  clientApp->Setup(serverAddress, packetSize, nPackets, pktInterval);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}