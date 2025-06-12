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
      .AddConstructor<CustomUdpServer>()
      .AddAttribute("Port", "Listening port",
                    UintegerValue(9),
                    MakeUintegerAccessor(&CustomUdpServer::m_port),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  CustomUdpServer() {}
  virtual ~CustomUdpServer() { m_socket = nullptr; }

private:
  void StartApplication(void)
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
    }
  }

  void StopApplication(void)
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
      NS_LOG_UNCOND("Server received packet of size " << packet->GetSize() << " from " << from);
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
      .AddConstructor<CustomUdpClient>()
      .AddAttribute("RemoteAddress1", "First server IPv4 address",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&CustomUdpClient::m_serverAddr1),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort1", "First server port",
                    UintegerValue(9),
                    MakeUintegerAccessor(&CustomUdpClient::m_serverPort1),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("RemoteAddress2", "Second server IPv4 address",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&CustomUdpClient::m_serverAddr2),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort2", "Second server port",
                    UintegerValue(9),
                    MakeUintegerAccessor(&CustomUdpClient::m_serverPort2),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("Interval", "Packet interval in seconds",
                    DoubleValue(1.0),
                    MakeDoubleAccessor(&CustomUdpClient::m_interval),
                    MakeDoubleChecker<double>())
      .AddAttribute("PacketSize", "Packet size in bytes",
                    UintegerValue(512),
                    MakeUintegerAccessor(&CustomUdpClient::m_pktSize),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  CustomUdpClient() {}
  virtual ~CustomUdpClient() {}

private:
  void StartApplication(void)
  {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &CustomUdpClient::SendPacket, this);
  }

  void StopApplication(void)
  {
    Simulator::Cancel(m_sendEvent);
  }

  void SendPacket(void)
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
    }

    // Send to first server
    Ptr<Packet> pkt1 = Create<Packet>(m_pktSize);
    m_socket->SendTo(pkt1, 0, InetSocketAddress(m_serverAddr1, m_serverPort1));

    // Send to second server
    Ptr<Packet> pkt2 = Create<Packet>(m_pktSize);
    m_socket->SendTo(pkt2, 0, InetSocketAddress(m_serverAddr2, m_serverPort2));

    NS_LOG_UNCOND("Client sent two packets of size " << m_pktSize);

    m_sendEvent = Simulator::Schedule(Seconds(m_interval), &CustomUdpClient::SendPacket, this);
  }

  Ipv4Address m_serverAddr1;
  uint16_t m_serverPort1;
  Ipv4Address m_serverAddr2;
  uint16_t m_serverPort2;
  double m_interval;
  uint32_t m_pktSize;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3); // 0: client, 1: server1, 2: server2

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(10000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install custom applications
  uint16_t serverPort1 = 4000;
  uint16_t serverPort2 = 4001;

  CustomUdpServerHelper server1(serverPort1);
  CustomUdpServerHelper server2(serverPort2);

  ApplicationContainer serverApps1 = server1.Install(nodes.Get(1));
  ApplicationContainer serverApps2 = server2.Install(nodes.Get(2));

  serverApps1.Start(Seconds(0.0));
  serverApps1.Stop(Seconds(10.0));
  serverApps2.Start(Seconds(0.0));
  serverApps2.Stop(Seconds(10.0));

  CustomUdpClientHelper client;
  client.SetAttribute("RemoteAddress1", Ipv4Value(interfaces.GetAddress(1)));
  client.SetAttribute("RemotePort1", UintegerValue(serverPort1));
  client.SetAttribute("RemoteAddress2", Ipv4Value(interfaces.GetAddress(2)));
  client.SetAttribute("RemotePort2", UintegerValue(serverPort2));
  client.SetAttribute("Interval", DoubleValue(1.0));
  client.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(0.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}