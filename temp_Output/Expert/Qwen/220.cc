#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpSocketExample");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpServer")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpServer>();
    return tid;
  }

  UdpServer() {}
  virtual ~UdpServer() {}

protected:
  void DoInitialize(void) override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
  }

  void DoDispose(void) override
  {
    m_socket->Close();
    Application::DoDispose();
  }

private:
  void StartApplication(void) override {}
  void StopApplication(void) override {}

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
    }
  }

  Ptr<Socket> m_socket;
};

class UdpClient : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpClient")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpClient>()
                            .AddAttribute("RemoteAddress",
                                          "The destination address of the UDP packets.",
                                          AddressValue(),
                                          MakeAddressAccessor(&UdpClient::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("Interval",
                                          "The time between two consecutive packets in seconds.",
                                          TimeValue(Seconds(1.0)),
                                          MakeTimeAccessor(&UdpClient::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("PacketSize",
                                          "The size of the UDP packets generated.",
                                          UintegerValue(512),
                                          MakeUintegerAccessor(&UdpClient::m_packetSize),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
  }

  UdpClient() {}
  virtual ~UdpClient() {}

protected:
  void DoInitialize(void) override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_sendEvent = EventId();
  }

  void DoDispose(void) override
  {
    m_socket->Close();
    Application::DoDispose();
  }

private:
  void StartApplication(void) override
  {
    ScheduleTransmit(Seconds(0.0));
  }

  void StopApplication(void) override
  {
    if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }
  }

  void ScheduleTransmit(Time dt)
  {
    m_sendEvent = Simulator::Schedule(dt, &UdpClient::SendPacket, this);
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->SendTo(packet, 0, m_peerAddress);

    NS_LOG_INFO("Sent packet of size " << m_packetSize << " to " << m_peerAddress);
    ScheduleTransmit(m_interval);
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  Time m_interval;
  uint32_t m_packetSize;
  EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::Ipv4GlobalRouting::PerflowDeficit", BooleanValue(true));

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

  UdpServerHelper serverApp;
  ApplicationContainer serverApps = serverApp.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper clientApp;
  clientApp.SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(interfaces.GetAddress(1), 9)));
  clientApp.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientApp.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = clientApp.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

class UdpClientHelper : public ApplicationHelper
{
public:
  UdpClientHelper()
  {
    m_factory.SetTypeId(UdpClient::GetTypeId());
  }
};

class UdpServerHelper : public ApplicationHelper
{
public:
  UdpServerHelper()
  {
    m_factory.SetTypeId(UdpServer::GetTypeId());
  }
};