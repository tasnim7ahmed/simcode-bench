#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

class SocketSender : public Application
{
public:
  static TypeId GetTypeId();
  SocketSender();
  virtual ~SocketSender();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void SendData();
  Ptr<Socket> m_socket;
  uint32_t m_totalBytesToSend;
  uint32_t m_bytesSent;
};

TypeId
SocketSender::GetTypeId()
{
  static TypeId tid = TypeId("SocketSender")
                          .SetParent<Application>()
                          .AddConstructor<SocketSender>();
  return tid;
}

SocketSender::SocketSender()
    : m_socket(nullptr), m_totalBytesToSend(0), m_bytesSent(0)
{
}

SocketSender::~SocketSender()
{
  m_socket = nullptr;
}

void SocketSender::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress remoteAddr = InetSocketAddress(Ipv4Address("10.1.2.2"), 8080);
    m_socket->Connect(remoteAddr);
    m_socket->SetConnectCallback(MakeCallback(&SocketSender::SendData, this),
                                 MakeNullCallback<void, Ptr<Socket>>());
  }
}

void SocketSender::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void SocketSender::SendData()
{
  while (m_bytesSent < m_totalBytesToSend)
  {
    uint32_t toSend = std::min<uint32_t>(m_totalBytesToSend - m_bytesSent, 1448); // MSS
    Ptr<Packet> packet = Create<Packet>(toSend);
    int sent = m_socket->Send(packet);
    if (sent < 0)
    {
      break;
    }
    m_bytesSent += sent;
  }
  if (m_bytesSent >= m_totalBytesToSend)
  {
    m_socket->ShutdownSend();
  }
}

static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
  Config::SetDefault("ns3::TcpSocketBase::MinRttWarning", BooleanValue(false));

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(Time("10ms")));

  NetDeviceContainer devices01, devices12;
  devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);
  address.SetBase("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 8080));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  SocketSenderHelper sender;
  sender.SetAttribute("TotalBytes", UintegerValue(2000000));
  ApplicationContainer senderApp = sender.Install(nodes.Get(0));
  senderApp.Start(Seconds(0.0));
  senderApp.Stop(Seconds(10.0));

  AsciiTraceHelper asciiTraceHelper;
  p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-large-transfer.tr"));
  p2p.EnablePcapAll("tcp-large-transfer");

  Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::SocketSender/m_socket/CongestionWindow",
                                MakeBoundCallback(&CwndChange, asciiTraceHelper.CreateFileStream("cwnd.dat")));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}