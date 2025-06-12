#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

class TcpSocketSender : public Application
{
public:
  static TypeId GetTypeId(void);
  TcpSocketSender();
  virtual ~TcpSocketSender();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_sendSize;
  uint64_t m_totalBytesToSend;
  uint64_t m_bytesSent;
  void SendData(Ptr<Socket>);
};

TypeId
TcpSocketSender::GetTypeId(void)
{
  static TypeId tid = TypeId("TcpSocketSender")
    .SetParent<Application>()
    .AddConstructor<TcpSocketSender>()
    .AddAttribute("SendSize", "The amount of data to send each write call (bytes)",
                  UintegerValue(1024),
                  MakeUintegerAccessor(&TcpSocketSender::m_sendSize),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("TotalBytes", "Total number of bytes to be sent",
                  UintegerValue(2000000),
                  MakeUintegerAccessor(&TcpSocketSender::m_totalBytesToSend),
                  MakeUintegerChecker<uint64_t>());
  return tid;
}

TcpSocketSender::TcpSocketSender()
  : m_sendSize(1024),
    m_totalBytesToSend(2000000),
    m_bytesSent(0)
{
}

TcpSocketSender::~TcpSocketSender()
{
}

void
TcpSocketSender::StartApplication(void)
{
  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      m_socket->Connect(m_peer);
      m_socket->ShutdownRecv();
      m_socket->SetConnectCallback(
        MakeCallback(&TcpSocketSender::SendData, this),
        MakeCallback(&TcpSocketSender::SendData, this));
    }
  SendData(m_socket);
}

void
TcpSocketSender::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
}

void
TcpSocketSender::SendData(Ptr<Socket> socket)
{
  while (m_bytesSent < m_totalBytesToSend)
    {
      uint32_t left = m_totalBytesToSend - m_bytesSent;
      uint32_t toSend = std::min(left, static_cast<uint64_t>(m_sendSize));
      Ptr<Packet> packet = Create<Packet>(toSend);
      int sent = socket->Send(packet);
      if (sent < 0)
        {
          return;
        }
      m_bytesSent += sent;
    }
}

static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(512));

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
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces12.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  TcpSocketSenderHelper* senderHelper = new TcpSocketSenderHelper();
  senderHelper->SetAttribute("TotalBytes", UintegerValue(2000000));
  ApplicationContainer sourceApps = senderHelper->Install(nodes.Get(0));
  sourceApps.Add(senderHelper->Install(nodes.Get(0)));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::TcpSocketSender/Socket/Tx", MakeCallback(&CwndChange, AsciiTraceHelper().CreateFileStream("tcp-large-transfer.cwnd")));

  AsciiTraceHelper asciiTraceHelper;
  p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-large-transfer.tr"));
  p2p.EnablePcapAll("tcp-large-transfer");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}