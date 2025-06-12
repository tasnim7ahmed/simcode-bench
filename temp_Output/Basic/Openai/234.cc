#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelayExample");

class UdpRelayApp : public Application
{
public:
  UdpRelayApp() : m_socket(0) {}
  virtual ~UdpRelayApp() { m_socket = 0; }

  void Setup(Address listenAddress, uint16_t listenPort, Address forwardAddress, uint16_t forwardPort)
  {
    m_listenAddress = listenAddress;
    m_listenPort = listenPort;
    m_forwardAddress = forwardAddress;
    m_forwardPort = forwardPort;
  }

private:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_listenAddress), m_listenPort);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpRelayApp::HandleRead, this));
    }
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      Ptr<Socket> forwardSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      forwardSocket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_forwardAddress), m_forwardPort));
      forwardSocket->Send(packet->Copy());
      forwardSocket->Close();
    }
  }

  Ptr<Socket> m_socket;
  Address m_listenAddress;
  uint16_t m_listenPort;
  Address m_forwardAddress;
  uint16_t m_forwardPort;
};

class UdpServerLoggerApp : public Application
{
public:
  UdpServerLoggerApp() : m_socket(0) {}
  virtual ~UdpServerLoggerApp() { m_socket = 0; }

  void Setup(Address listenAddress, uint16_t listenPort)
  {
    m_listenAddress = listenAddress;
    m_listenPort = listenPort;
  }

private:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_listenAddress), m_listenPort);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServerLoggerApp::HandleRead, this));
    }
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      uint8_t buffer[1024];
      uint32_t len = packet->CopyData(buffer, sizeof(buffer));
      std::string msg(reinterpret_cast<char*>(buffer), len);
      NS_LOG_UNCOND("Server received: " << msg << " at " << Simulator::Now().GetSeconds() << "s");
    }
  }

  Ptr<Socket> m_socket;
  Address m_listenAddress;
  uint16_t m_listenPort;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpRelayExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // 0: client, 1: relay, 2: server

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer if01 = ipv4.Assign(dev01);

  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = ipv4.Assign(dev12);

  // App settings
  uint16_t clientPort = 50000;
  uint16_t relayPort = 50000;
  uint16_t serverPort = 9;

  // UDP client socket: send to relay
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
  clientSocket->Connect(InetSocketAddress(if01.GetAddress(1), relayPort));

  // Simulate client sending a packet
  Simulator::Schedule(Seconds(1.0), [&clientSocket]() {
    std::string msg = "Hello from client!";
    Ptr<Packet> packet = Create<Packet>((uint8_t*)msg.data(), msg.size());
    clientSocket->Send(packet);
  });

  // UDP Relay app on node 1
  Ptr<UdpRelayApp> relayApp = CreateObject<UdpRelayApp>();
  relayApp->Setup(if01.GetAddress(1), relayPort, if12.GetAddress(1), serverPort);
  nodes.Get(1)->AddApplication(relayApp);
  relayApp->SetStartTime(Seconds(0.5));
  relayApp->SetStopTime(Seconds(10.0));

  // UDP Server logger on node 2
  Ptr<UdpServerLoggerApp> serverApp = CreateObject<UdpServerLoggerApp>();
  serverApp->Setup(if12.GetAddress(1), serverPort);
  nodes.Get(2)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}