#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpClientServer");

class CustomUdpServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("CustomUdpServer")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<CustomUdpServer>()
      .AddAttribute("Port", "Listening port",
                    UintegerValue(0),
                    MakeUintegerAccessor(&CustomUdpServer::m_port),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  CustomUdpServer() {}
  virtual ~CustomUdpServer() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    if (m_socket->Bind(local) == -1)
      {
        NS_FATAL_ERROR("Failed to bind socket");
      }
    m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
      {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
      }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
      {
        NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
      }
  }

  uint16_t m_port;
  Ptr<Socket> m_socket;
};

class CustomUdpClient : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("CustomUdpClient")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<CustomUdpClient>()
      .AddAttribute("RemoteAddress1", "First server address",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&CustomUdpClient::m_serverAddr1),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort1", "First server port",
                    UintegerValue(0),
                    MakeUintegerAccessor(&CustomUdpClient::m_serverPort1),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("RemoteAddress2", "Second server address",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&CustomUdpClient::m_serverAddr2),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort2", "Second server port",
                    UintegerValue(0),
                    MakeUintegerAccessor(&CustomUdpClient::m_serverPort2),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("Interval", "Interval between packets",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&CustomUdpClient::m_interval),
                    MakeTimeChecker())
      .AddAttribute("PacketSize", "Size of packets",
                    UintegerValue(512),
                    MakeUintegerAccessor(&CustomUdpClient::m_packetSize),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  CustomUdpClient() {}
  virtual ~CustomUdpClient() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);

    m_sendEvent = Simulator::ScheduleNow(&CustomUdpClient::SendPacket, this);
  }

  virtual void StopApplication(void)
  {
    Simulator::Cancel(m_sendEvent);
    if (m_socket)
      {
        m_socket->Close();
      }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->SendTo(packet, 0, InetSocketAddress(m_serverAddr1, m_serverPort1));
    m_socket->SendTo(packet, 0, InetSocketAddress(m_serverAddr2, m_serverPort2));
    NS_LOG_INFO("Sent packet to both servers");

    m_sendEvent = Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
  }

  Ipv4Address m_serverAddr1;
  uint16_t m_serverPort1;
  Ipv4Address m_serverAddr2;
  uint16_t m_serverPort2;
  Time m_interval;
  uint32_t m_packetSize;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("CustomUdpClientServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // 0: client, 1: server1, 2: server2

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Server applications
  uint16_t serverPort1 = 9;
  uint16_t serverPort2 = 10;

  CustomUdpServerHelper serverApp1(serverPort1);
  CustomUdpServerHelper serverApp2(serverPort2);

  ApplicationContainer serverApps1 = serverApp1.Install(nodes.Get(1));
  ApplicationContainer serverApps2 = serverApp2.Install(nodes.Get(2));

  serverApps1.Start(Seconds(0.0));
  serverApps1.Stop(Seconds(10.0));
  serverApps2.Start(Seconds(0.0));
  serverApps2.Stop(Seconds(10.0));

  // Client application
  CustomUdpClientHelper clientAppHelper;
  clientAppHelper.SetAttribute("RemoteAddress1", Ipv4Value(interfaces.GetAddress(1)));
  clientAppHelper.SetAttribute("RemotePort1", UintegerValue(serverPort1));
  clientAppHelper.SetAttribute("RemoteAddress2", Ipv4Value(interfaces.GetAddress(2)));
  clientAppHelper.SetAttribute("RemotePort2", UintegerValue(serverPort2));
  clientAppHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientAppHelper.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = clientAppHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}