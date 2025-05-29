#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttExample");

void
RttTracer(Ptr<Socket> socket, Time oldRtt, Time newRtt)
{
  std::cout << Simulator::Now().GetSeconds()
            << "s: RTT = " << newRtt.GetMilliSeconds()
            << " ms" << std::endl;
}

class TcpRttApp : public Application
{
public:
  TcpRttApp();
  virtual ~TcpRttApp();

  void SetSocket(Ptr<Socket> socket);
  void SetRemote(Address address);
  void SetSendSize(uint32_t size);
  void SetNumPackets(uint32_t count);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_sendSize;
  uint32_t        m_numPackets;
  uint32_t        m_sent;
  EventId         m_sendEvent;
};

TcpRttApp::TcpRttApp()
  : m_socket(0),
    m_peer(),
    m_sendSize(0),
    m_numPackets(0),
    m_sent(0)
{
}

TcpRttApp::~TcpRttApp()
{
  m_socket = 0;
}

void
TcpRttApp::SetSocket(Ptr<Socket> socket)
{
  m_socket = socket;
}

void
TcpRttApp::SetRemote(Address address)
{
  m_peer = address;
}

void
TcpRttApp::SetSendSize(uint32_t size)
{
  m_sendSize = size;
}

void
TcpRttApp::SetNumPackets(uint32_t count)
{
  m_numPackets = count;
}

void
TcpRttApp::StartApplication(void)
{
  m_sent = 0;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  m_sendEvent = Simulator::Schedule(Seconds(0.1), &TcpRttApp::SendPacket, this);
}

void
TcpRttApp::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
    }
  Simulator::Cancel(m_sendEvent);
}

void
TcpRttApp::SendPacket(void)
{
  if (m_sent < m_numPackets)
    {
      Ptr<Packet> packet = Create<Packet>(m_sendSize);
      m_socket->Send(packet);
      ++m_sent;
      m_sendEvent = Simulator::Schedule(Seconds(0.05), &TcpRttApp::SendPacket, this);
    }
}

int main(int argc, char *argv[])
{
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8080;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  // Server
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  // Client TCPSocket
  Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  tcpSocket->SetAttribute("SndBufSize", UintegerValue(65536));
  tcpSocket->SetAttribute("RcvBufSize", UintegerValue(65536));

  // RTT Tracing
  tcpSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTracer));

  // Custom application to send data
  Ptr<TcpRttApp> app = CreateObject<TcpRttApp>();
  app->SetSocket(tcpSocket);
  app->SetRemote(serverAddress);
  app->SetSendSize(1024); // bytes
  app->SetNumPackets(100);

  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(2.0));
  app->SetStopTime(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}