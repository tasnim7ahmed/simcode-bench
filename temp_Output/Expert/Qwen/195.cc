#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferSimulation");

class TcpBulkSender : public Application
{
public:
  static TypeId GetTypeId(void);
  TcpBulkSender();
  virtual ~TcpBulkSender();

protected:
  void StartApplication(void);
  void StopApplication(void);

private:
  void SendData(void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_totalBytes;
  uint32_t m_bytesSent;
};

TypeId
TcpBulkSender::GetTypeId(void)
{
  static TypeId tid = TypeId("TcpBulkSender")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<TcpBulkSender>()
                          .AddAttribute("RemoteAddress",
                                        "The destination Address of the outbound packets",
                                        AddressValue(),
                                        MakeAddressAccessor(&TcpBulkSender::m_peer),
                                        MakeAddressChecker())
                          .AddAttribute("PacketSize",
                                        "The size of the packet to send in bytes",
                                        UintegerValue(1024),
                                        MakeUintegerAccessor(&TcpBulkSender::m_packetSize),
                                        MakeUintegerChecker<uint32_t>(1))
                          .AddAttribute("TotalBytes",
                                        "The total number of bytes to send",
                                        UintegerValue(0xFFFFFFFF),
                                        MakeUintegerAccessor(&TcpBulkSender::m_totalBytes),
                                        MakeUintegerChecker<uint32_t>());
  return tid;
}

TcpBulkSender::TcpBulkSender()
    : m_socket(nullptr), m_packetSize(512), m_totalBytes(0xFFFFFFFF), m_bytesSent(0)
{
}

TcpBulkSender::~TcpBulkSender()
{
  m_socket = nullptr;
}

void TcpBulkSender::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);

    if (InetSocketAddress::IsMatchingType(m_peer))
    {
      NS_LOG_LOGIC("Connecting to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetPort());
    }

    m_socket->Connect(m_peer);
    m_socket->TraceConnectWithoutContext("Tx", MakeCallback(&TcpBulkSender::SendData, this));
  }
}

void TcpBulkSender::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_socket->SetSendCallback(MakeNullCallback<void, Ptr<Socket>, uint32_t>());
  }
}

void TcpBulkSender::SendData(void)
{
  while (m_bytesSent < m_totalBytes)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    int amountSent = m_socket->Send(packet);
    if (amountSent < 0)
    {
      break;
    }
    m_bytesSent += amountSent;
  }
}

int main(int argc, char *argv[])
{
  std::string rate = "1Mbps";
  std::string delay = "20ms";
  uint32_t packetSize = 1024;
  uint32_t totalBytes = 10485760; // 10 MB

  CommandLine cmd(__FILE__);
  cmd.AddValue("rate", "P2P Data Rate", rate);
  cmd.AddValue("delay", "P2P Delay", delay);
  cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue("totalBytes", "Total number of bytes to send", totalBytes);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(rate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  TcpBulkSenderHelper senderHelper("ns3::TcpSocketFactory", sinkAddress);
  senderHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  senderHelper.SetAttribute("TotalBytes", UintegerValue(totalBytes));
  ApplicationContainer clientApps = senderHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("tcp_bulk_transfer");

  Config::Connect("/NodeList/0/ApplicationList/0/$TcpBulkSender/Tx", MakeCallback([](Ptr<const Packet> p) {
    static uint64_t totalBytesTransferred = 0;
    static Time lastLogTime = Seconds(0);
    Time now = Simulator::Now();
    totalBytesTransferred += p->GetSize();
    double throughput =
        static_cast<double>(totalBytesTransferred * 8) / (now.GetSeconds() - lastLogTime.GetSeconds()) / 1e6;
    if (now.GetSeconds() - lastLogTime.GetSeconds() >= 1.0)
    {
      NS_LOG_UNCOND("[" << now.As(Time::S) << "] Bytes Transferred: " << totalBytesTransferred
                         << " bytes, Throughput: " << throughput << " Mbps");
      lastLogTime = now;
      totalBytesTransferred = 0;
    }
  }));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}