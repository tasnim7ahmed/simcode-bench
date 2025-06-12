#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelaySimulation");

class UdpClient : public Application
{
public:
  static TypeId GetTypeId();
  UdpClient();
  virtual ~UdpClient();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void SendPacket();
  Ptr<Socket> m_socket;
  Address m_relayAddress;
  uint16_t m_port;
  EventId m_sendEvent;
};

class UdpRelay : public Application
{
public:
  static TypeId GetTypeId();
  UdpRelay();
  virtual ~UdpRelay();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void HandleRead(Ptr<Socket> socket);
  void ForwardPacket(Ptr<Packet> packet, const Address& fromAddress);
  Ptr<Socket> m_udpSocket;
  Address m_serverAddress;
};

class UdpServer : public Application
{
public:
  static TypeId GetTypeId();
  UdpServer();
  virtual ~UdpServer();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void HandleRead(Ptr<Socket> socket);
  Ptr<Socket> m_udpSocket;
};

TypeId UdpClient::GetTypeId()
{
  static TypeId tid = TypeId("UdpClient")
    .SetParent<Application>()
    .AddConstructor<UdpClient>()
    .AddAttribute("RemoteAddress", "The relay's address",
                  AddressValue(),
                  MakeAddressAccessor(&UdpClient::m_relayAddress),
                  MakeAddressChecker())
    .AddAttribute("Port", "The destination port",
                  UintegerValue(9),
                  MakeUintegerAccessor(&UdpClient::m_port),
                  MakeUintegerChecker<uint16_t>());
  return tid;
}

UdpClient::UdpClient()
{
}

UdpClient::~UdpClient()
{
  m_sendEvent.Cancel();
}

void UdpClient::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_relayAddress);

  SendPacket();
}

void UdpClient::StopApplication()
{
  m_sendEvent.Cancel();
}

void UdpClient::SendPacket()
{
  std::string message = "Hello from client";
  Ptr<Packet> packet = Create<Packet>((uint8_t*)message.c_str(), message.size());
  m_socket->Send(packet);

  NS_LOG_INFO("Client sent packet with message: " << message);
}

TypeId UdpRelay::GetTypeId()
{
  static TypeId tid = TypeId("UdpRelay")
    .SetParent<Application>()
    .AddConstructor<UdpRelay>()
    .AddAttribute("LocalAddress", "The local address to bind to",
                  AddressValue(),
                  MakeAddressAccessor(&UdpRelay::m_localAddress),
                  MakeAddressChecker())
    .AddAttribute("ServerAddress", "The server's address",
                  AddressValue(),
                  MakeAddressAccessor(&UdpRelay::m_serverAddress),
                  MakeAddressChecker());
  return tid;
}

UdpRelay::UdpRelay()
{
}

UdpRelay::~UdpRelay()
{
}

void UdpRelay::StartApplication()
{
  m_udpSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress::ConvertFrom(m_localAddress);
  m_udpSocket->Bind(local);
  m_udpSocket->SetRecvCallback(MakeCallback(&UdpRelay::HandleRead, this));
}

void UdpRelay::StopApplication()
{
  if (m_udpSocket)
    {
      m_udpSocket->Close();
      m_udpSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void UdpRelay::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Relay received a packet of size " << packet->GetSize());
      ForwardPacket(packet, from);
    }
}

void UdpRelay::ForwardPacket(Ptr<Packet> packet, const Address& fromAddress)
{
  if (!m_udpSocket)
    return;

  m_udpSocket->SendTo(packet, 0, m_serverAddress);
  NS_LOG_INFO("Relay forwarded packet to server");
}

TypeId UdpServer::GetTypeId()
{
  static TypeId tid = TypeId("UdpServer")
    .SetParent<Application>()
    .AddConstructor<UdpServer>()
    .AddAttribute("LocalAddress", "The local address to bind to",
                  AddressValue(),
                  MakeAddressAccessor(&UdpServer::m_localAddress),
                  MakeAddressChecker());
  return tid;
}

UdpServer::UdpServer()
{
}

UdpServer::~UdpServer()
{
}

void UdpServer::StartApplication()
{
  m_udpSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress::ConvertFrom(m_localAddress);
  m_udpSocket->Bind(local);
  m_udpSocket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication()
{
  if (m_udpSocket)
    {
      m_udpSocket->Close();
      m_udpSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      char *buffer = new char[packet->GetSize() + 1];
      packet->CopyData((uint8_t*)buffer, packet->GetSize());
      buffer[packet->GetSize()] = '\0';
      NS_LOG_INFO("Server received packet: \"" << buffer << "\"");
      delete [] buffer;
    }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3); // 0:client, 1:relay, 2:server

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devClientRelay = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devRelayServer = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifClientRelay = ipv4.Assign(devClientRelay);
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifRelayServer = ipv4.Assign(devRelayServer);

  // Client application
  UdpClientHelper clientAppHelper;
  clientAppHelper.SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(ifRelayServer.GetAddress(0), 9)));
  ApplicationContainer clientApp = clientAppHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  // Relay application
  UdpRelayHelper relayAppHelper;
  relayAppHelper.SetAttribute("LocalAddress", AddressValue(InetSocketAddress(ifRelayServer.GetAddress(0), 9)));
  relayAppHelper.SetAttribute("ServerAddress", AddressValue(InetSocketAddress(ifRelayServer.GetAddress(1), 9)));
  ApplicationContainer relayApp = relayAppHelper.Install(nodes.Get(1));
  relayApp.Start(Seconds(0.0));
  relayApp.Stop(Seconds(10.0));

  // Server application
  UdpServerHelper serverAppHelper;
  serverAppHelper.SetAttribute("LocalAddress", AddressValue(InetSocketAddress(Ipv4Address::GetAny(), 9)));
  ApplicationContainer serverApp = serverAppHelper.Install(nodes.Get(2));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}