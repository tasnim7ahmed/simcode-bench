#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class TcpServerApp : public Application
{
public:
  TcpServerApp() : m_socket(0) {}
  virtual ~TcpServerApp() { m_socket = 0; }

  void Setup(Address address)
  {
    m_local = address;
  }

private:
  virtual void StartApplication() override
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

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void HandleAccept(Ptr<Socket> s, const Address &from)
  {
    s->SetRecvCallback(MakeCallback(&TcpServerApp::HandleRead, this));
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() > 0)
      {
        std::cout << "Server received " << packet->GetSize() << " bytes at "
                  << Simulator::Now().GetSeconds() << "s" << std::endl;
      }
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
};

class TcpClientApp : public Application
{
public:
  TcpClientApp() : m_socket(0), m_sendSize(512) {}
  virtual ~TcpClientApp() { m_socket = 0; }

  void Setup(Address address, uint32_t sendSize)
  {
    m_peer = address;
    m_sendSize = sendSize;
  }

private:
  virtual void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    Simulator::Schedule(Seconds(0.1), &TcpClientApp::Send, this);
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void Send()
  {
    Ptr<Packet> packet = Create<Packet>(m_sendSize);
    m_socket->Send(packet);
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_sendSize;
};

int main(int argc, char *argv[])
{
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  Address clientAddress(InetSocketAddress(interfaces.GetAddress(0), port));

  // Server
  Ptr<TcpServerApp> serverApp = CreateObject<TcpServerApp>();
  serverApp->Setup(InetSocketAddress(Ipv4Address::GetAny(), port));
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(2.0));

  // Client
  Ptr<TcpClientApp> clientApp = CreateObject<TcpClientApp>();
  clientApp->Setup(serverAddress, 128);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(0.2));
  clientApp->SetStopTime(Seconds(2.0));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}