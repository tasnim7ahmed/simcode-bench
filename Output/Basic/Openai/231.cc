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

  void Setup(Address address)
  {
    m_local = address;
  }

private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->Listen();
    }
    m_socket->SetAcceptCallback(
        MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
        MakeCallback(&TcpServerApp::HandleAccept, this));
    m_socket->SetRecvCallback(MakeCallback(&TcpServerApp::HandleRead, this));
  }

  virtual void StopApplication(void)
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
      NS_LOG_UNCOND("Server received " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s");
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
};

class TcpClientApp : public Application
{
public:
  TcpClientApp() : m_socket(0), m_peer(), m_data(0), m_dataSize(0) {}

  void Setup(Address address, uint8_t *data, uint32_t dataSize)
  {
    m_peer = address;
    m_data = data;
    m_dataSize = dataSize;
  }

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    Simulator::Schedule(Seconds(0.1), &TcpClientApp::SendData, this);
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
    if (m_data)
    {
      delete[] m_data;
      m_data = nullptr;
    }
  }

  void SendData()
  {
    Ptr<Packet> packet = Create<Packet>(m_data, m_dataSize);
    m_socket->Send(packet);
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint8_t *m_data;
  uint32_t m_dataSize;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

  uint16_t port = 50000;

  Ptr<TcpServerApp> serverApp = CreateObject<TcpServerApp>();
  Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  serverApp->Setup(serverAddress);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(2.0));

  uint32_t dataSize = 100;
  uint8_t *data = new uint8_t[dataSize];
  for (uint32_t i = 0; i < dataSize; ++i)
    data[i] = i % 256;

  Ptr<TcpClientApp> clientApp = CreateObject<TcpClientApp>();
  Address peerAddress(InetSocketAddress(ifaces.GetAddress(1), port));
  clientApp->Setup(peerAddress, data, dataSize);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(0.2));
  clientApp->SetStopTime(Seconds(2.0));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}