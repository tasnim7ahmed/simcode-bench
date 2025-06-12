#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RawUdpSocketExample");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId();
  UdpServer();
  virtual ~UdpServer();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
};

TypeId
UdpServer::GetTypeId()
{
  static TypeId tid = TypeId("UdpServer")
                          .SetParent<Application>()
                          .SetGroupName("Applications")
                          .AddConstructor<UdpServer>();
  return tid;
}

UdpServer::UdpServer()
    : m_socket(nullptr)
{
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

void
UdpServer::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
  }
}

void
UdpServer::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
  }
}

class UdpClient : public Application
{
public:
  static TypeId GetTypeId();
  UdpClient();
  virtual ~UdpClient();

  void SetRemote(Address address);

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void SendPacket();

  Address m_peerAddress;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  uint32_t m_packetSize;
  Time m_interval;
};

TypeId
UdpClient::GetTypeId()
{
  static TypeId tid = TypeId("UdpClient")
                          .SetParent<Application>()
                          .SetGroupName("Applications")
                          .AddConstructor<UdpClient>();
  return tid;
}

UdpClient::UdpClient()
    : m_peerAddress(),
      m_socket(nullptr),
      m_sendEvent(),
      m_packetSize(1024),
      m_interval(Seconds(1.))
{
}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
}

void
UdpClient::SetRemote(Address address)
{
  m_peerAddress = address;
}

void
UdpClient::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Connect(m_peerAddress);
  }

  SendPacket();
}

void
UdpClient::StopApplication()
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void
UdpClient::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  NS_LOG_INFO("Sent packet of size " << m_packetSize << " at time " << Simulator::Now().As(Time::S));

  m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("RawUdpSocketExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install server on node 1
  UdpServerHelper serverHelper;
  ApplicationContainer serverApp = serverHelper.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Install client on node 0
  UdpClientHelper clientHelper;
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetRemote(AddressValue(InetSocketAddress(interfaces.GetAddress(1), 9)));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}