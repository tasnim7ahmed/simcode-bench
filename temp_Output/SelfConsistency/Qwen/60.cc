#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

class TCPSocketSender : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("TCPSocketSender")
                            .SetParent<Application>()
                            .AddConstructor<TCPSocketSender>()
                            .AddAttribute("Remote", "The address of the destination",
                                          AddressValue(),
                                          MakeAddressAccessor(&TCPSocketSender::m_peer),
                                          MakeAddressChecker())
                            .AddAttribute("SendSize", "The amount of data to send (in bytes)",
                                          UintegerValue(2000000),
                                          MakeUintegerAccessor(&TCPSocketSender::m_sendSize),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
  }

  TCPSocketSender() : m_socket(nullptr), m_totalDataSent(0) {}
  virtual ~TCPSocketSender();

private:
  void StartApplication(void);
  void StopApplication(void);
  void SendData(Ptr<Socket> socket);

  Address m_peer;
  uint32_t m_sendSize;
  Ptr<Socket> m_socket;
  uint32_t m_totalDataSent;
};

TCPSocketSender::~TCPSocketSender()
{
  m_socket = nullptr;
}

void TCPSocketSender::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);

    if (m_socket->Bind() == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }

    m_socket->Connect(m_peer);
    m_socket->SetConnectCallback(MakeCallback(&TCPSocketSender::SendData, this),
                                 MakeNullCallback<void, Ptr<Socket>>());
  }
  SendData(m_socket);
}

void TCPSocketSender::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void TCPSocketSender::SendData(Ptr<Socket> socket)
{
  while (m_totalDataSent < m_sendSize)
  {
    uint32_t left = m_sendSize - m_totalDataSent;
    uint32_t toSend = std::min(left, socket->GetTxAvailable());
    if (toSend == 0)
    {
      NS_LOG_INFO("TCP socket buffer full, waiting for space...");
      return;
    }

    Ptr<Packet> packet = Create<Packet>(toSend);
    int sent = socket->Send(packet);
    if (sent < 0)
    {
      NS_LOG_ERROR("Error sending packet: " << sent);
      break;
    }
    m_totalDataSent += sent;
  }

  NS_LOG_INFO("Finished sending " << m_totalDataSent << " bytes.");
}

static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = ipv4.Assign(devices01);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), 8080));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  TCPSocketSenderHelper sender;
  sender.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces12.GetAddress(1), 8080)));
  sender.SetAttribute("SendSize", UintegerValue(2000000));
  ApplicationContainer senderApp = sender.Install(nodes.Get(0));
  senderApp.Start(Seconds(0.0));
  senderApp.Stop(Seconds(10.0));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("tcp-large-transfer.cwnd");
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndChange, cwndStream));

  p2p.EnablePcapAll("tcp-large-transfer", false);
  p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-large-transfer.tr"));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}