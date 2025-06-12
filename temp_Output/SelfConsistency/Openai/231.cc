#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Server application: receives data and logs reception
class TcpServerApp : public Application
{
public:
  TcpServerApp();
  virtual ~TcpServerApp();

  void Setup(Address address);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

  void HandleAccept(Ptr<Socket> s, const Address& from);
  void HandleRead(Ptr<Socket> sock);

  Ptr<Socket> m_socket;
  Address m_local;
};

TcpServerApp::TcpServerApp() : m_socket(0)
{
}

TcpServerApp::~TcpServerApp()
{
  m_socket = 0;
}

void TcpServerApp::Setup(Address address)
{
  m_local = address;
}

void TcpServerApp::StartApplication()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->Listen();
      m_socket->SetAcceptCallback(
          MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
          MakeCallback(&TcpServerApp::HandleAccept, this));
    }
}

void TcpServerApp::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
    }
}

void TcpServerApp::HandleAccept(Ptr<Socket> s, const Address& from)
{
  s->SetRecvCallback(MakeCallback(&TcpServerApp::HandleRead, this));
}

void TcpServerApp::HandleRead(Ptr<Socket> sock)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = sock->RecvFrom(from)))
    {
      uint32_t nBytes = packet->GetSize();
      if (nBytes > 0)
        {
          NS_LOG_UNCOND("Server received " << nBytes << " bytes at "
                          << Simulator::Now().GetSeconds() << "s");
        }
    }
}

// Client application: sends a small amount of data
class TcpClientApp : public Application
{
public:
  TcpClientApp();
  virtual ~TcpClientApp();

  void Setup(Address address, uint32_t packetSize);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  bool m_sent;
};

TcpClientApp::TcpClientApp() : m_socket(0), m_packetSize(0), m_sent(false)
{
}

TcpClientApp::~TcpClientApp()
{
  m_socket = 0;
}

void TcpClientApp::Setup(Address address, uint32_t packetSize)
{
  m_peer = address;
  m_packetSize = packetSize;
}

void TcpClientApp::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
  m_socket->Connect(m_peer);
  Simulator::Schedule(Seconds(0.1), &TcpClientApp::SendPacket, this);
}

void TcpClientApp::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
    }
}

void TcpClientApp::SendPacket()
{
  if (!m_sent)
    {
      Ptr<Packet> packet = Create<Packet>(m_packetSize);
      m_socket->Send(packet);
      m_sent = true;
    }
}

int main(int argc, char *argv[])
{
  LogComponentEnable("TcpServerApp", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Setup point-to-point
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer devices = p2p.Install(nodes);

  // Install stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Server app on node 1
  uint16_t port = 50000;
  Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  Ptr<TcpServerApp> serverApp = CreateObject<TcpServerApp>();
  serverApp->Setup(bindAddress);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(2.0));

  // Client app on node 0
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  Ptr<TcpClientApp> clientApp = CreateObject<TcpClientApp>();
  clientApp->Setup(serverAddress, 100); // 100 bytes
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(0.5));
  clientApp->SetStopTime(Seconds(2.0));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}