#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpSocketExample");

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ServerApplication")
      .SetParent<Application>()
      .AddConstructor<ServerApplication>();
    return tid;
  }

  ServerApplication() {}
  virtual ~ServerApplication() {}

private:
  virtual void StartApplication()
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&ServerApplication::HandleRead, this));
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("Server received packet of size " << packet->GetSize());
    }
  }

  Ptr<Socket> m_socket;
};

class ClientApplication : public Application
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ClientApplication")
      .SetParent<Application>()
      .AddConstructor<ClientApplication>()
      .AddAttribute("RemoteAddress", "The destination address of the packets",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&ClientApplication::m_remoteAddress),
                    MakeIpv4AddressChecker())
      .AddAttribute("Interval", "The time between packets",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&ClientApplication::m_interval),
                    MakeTimeChecker());
    return tid;
  }

  ClientApplication() {}
  virtual ~ClientApplication() {}

private:
  virtual void StartApplication()
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Connect(InetSocketAddress(m_remoteAddress, 9));
    SendPacket();
  }

  virtual void StopApplication()
  {
    if (m_event.IsRunning())
    {
      Simulator::Cancel(m_event);
    }
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(1024); // 1024-byte packet
    m_socket->Send(packet);
    NS_LOG_UNCOND("Client sent packet to server at " << m_remoteAddress);
    m_event = Simulator::Schedule(m_interval, &ClientApplication::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_remoteAddress;
  Time m_interval;
  EventId m_event;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

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

  ApplicationContainer serverApp;
  ServerApplicationHelper serverAppHelper;
  serverApp.Add(serverAppHelper.Install(nodes.Get(1)));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  ApplicationContainer clientApp;
  ClientApplicationHelper clientAppHelper;
  clientAppHelper.SetAttribute("RemoteAddress", Ipv4Value(interfaces.GetAddress(1)));
  clientAppHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientApp.Add(clientAppHelper.Install(nodes.Get(0)));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}