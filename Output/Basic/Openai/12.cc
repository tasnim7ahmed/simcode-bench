#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <limits>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceApp");

class TraceBasedUdpClient : public Application
{
public:
  TraceBasedUdpClient();
  virtual ~TraceBasedUdpClient();

  void SetRemote(Address ip, uint16_t port);
  void SetTraceFile(const std::string& traceFile);
  void SetPacketSize(uint32_t size);

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void ScheduleNextTx();
  void SendPacket();
  void ParseTraceFile();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  std::string m_traceFileName;
  uint32_t m_pktSize;
  std::vector<Time> m_sendTimes;
  uint32_t m_nextSendIndex;
  EventId m_sendEvent;
  bool m_running;
};

TraceBasedUdpClient::TraceBasedUdpClient()
  : m_socket(nullptr),
    m_peerPort(0),
    m_pktSize(1472),
    m_nextSendIndex(0),
    m_running(false)
{
}

TraceBasedUdpClient::~TraceBasedUdpClient()
{
  m_socket = nullptr;
}

void TraceBasedUdpClient::SetRemote(Address ip, uint16_t port)
{
  m_peerAddress = ip;
  m_peerPort = port;
}

void TraceBasedUdpClient::SetTraceFile(const std::string& traceFile)
{
  m_traceFileName = traceFile;
}

void TraceBasedUdpClient::SetPacketSize(uint32_t size)
{
  m_pktSize = size;
}

void TraceBasedUdpClient::ParseTraceFile()
{
  m_sendTimes.clear();
  std::ifstream infile(m_traceFileName, std::ios_base::in);
  if (!infile.is_open())
  {
    NS_FATAL_ERROR("Failed to open trace file: " << m_traceFileName);
  }

  std::string line;
  while (std::getline(infile, line))
  {
    std::istringstream iss(line);
    double timestamp = 0.0;
    if (!(iss >> timestamp))
    {
      continue;
    }
    m_sendTimes.push_back(Seconds(timestamp));
  }
  infile.close();
}

void TraceBasedUdpClient::StartApplication()
{
  m_running = true;
  m_nextSendIndex = 0;

  if (InetSocketAddress::IsMatchingType(m_peerAddress))
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

  m_socket->Connect(m_peerAddress);

  ParseTraceFile();

  if (!m_sendTimes.empty())
  {
    Time firstSend = m_sendTimes[0];
    m_sendEvent = Simulator::Schedule(firstSend, &TraceBasedUdpClient::SendPacket, this);
  }
}

void TraceBasedUdpClient::StopApplication()
{
  m_running = false;
  if (m_sendEvent.IsRunning())
    Simulator::Cancel(m_sendEvent);
  if (m_socket)
    m_socket->Close();
}

void TraceBasedUdpClient::SendPacket()
{
  if (!m_running)
    return;
  Ptr<Packet> pkt = Create<Packet>(m_pktSize);
  m_socket->Send(pkt);

  NS_LOG_INFO("TraceBasedUdpClient sent packet of " << m_pktSize << " bytes at " << Simulator::Now().GetSeconds() << "s");

  ++m_nextSendIndex;
  if (m_nextSendIndex < m_sendTimes.size())
  {
    Time delta = m_sendTimes[m_nextSendIndex] - m_sendTimes[m_nextSendIndex - 1];
    m_sendEvent = Simulator::Schedule(delta, &TraceBasedUdpClient::SendPacket, this);
  }
}

class LoggingUdpServer : public Application
{
public:
  LoggingUdpServer();
  virtual ~LoggingUdpServer();

  void SetPort(uint16_t port);

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

LoggingUdpServer::LoggingUdpServer()
  : m_socket(nullptr),
    m_port(4000)
{
}

LoggingUdpServer::~LoggingUdpServer()
{
  m_socket = nullptr;
}

void LoggingUdpServer::SetPort(uint16_t port)
{
  m_port = port;
}

void LoggingUdpServer::StartApplication()
{
  TypeId tid = UdpSocketFactory::GetTypeId();
  m_socket = Socket::CreateSocket(GetNode(), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  if (GetNode()->GetObject<Ipv6>() != nullptr &&
      m_socket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), m_port)) == 0)
  {
    // IPv6 binding succeeded, do nothing
  }
  else
  {
    m_socket->Bind(local);
  }
  m_socket->SetRecvCallback(MakeCallback(&LoggingUdpServer::HandleRead, this));
}

void LoggingUdpServer::StopApplication()
{
  if (m_socket)
    m_socket->Close();
}

void LoggingUdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    uint32_t pktSize = packet->GetSize();
    NS_LOG_INFO("LoggingUdpServer received packet of " << pktSize << " bytes at " << Simulator::Now().GetSeconds() << "s");
  }
}

int main(int argc, char* argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;
  std::string traceFile = "trace.txt";

  CommandLine cmd;
  cmd.AddValue("ipv6", "Enable IPv6 instead of IPv4", useIpv6);
  cmd.AddValue("logging", "Enable logging output", enableLogging);
  cmd.AddValue("traceFile", "Trace file with packet times (seconds)", traceFile);
  cmd.Parse(argc, argv);

  if (enableLogging)
  {
    LogComponentEnable("CsmaUdpTraceApp", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TraceBasedUdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("LoggingUdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer ifaces4;
  Ipv6InterfaceContainer ifaces6;
  Address remoteAddr;

  if (!useIpv6)
  {
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ifaces4 = ipv4.Assign(devices);
    remoteAddr = InetSocketAddress(ifaces4.GetAddress(1), 4000);
  }
  else
  {
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    ifaces6 = ipv6.Assign(devices);
    ifaces6.SetForwarding(0, true);
    ifaces6.SetForwarding(1, true);
    ifaces6.SetDefaultRouteInAllNodes(0);
    remoteAddr = Inet6SocketAddress(ifaces6.GetAddress(1, 1), 4000);
  }

  // UDP server on n1
  Ptr<LoggingUdpServer> udpServer = CreateObject<LoggingUdpServer>();
  udpServer->SetPort(4000);
  nodes.Get(1)->AddApplication(udpServer);
  udpServer->SetStartTime(Seconds(1.0));
  udpServer->SetStopTime(Seconds(10.0));

  // UDP client on n0
  Ptr<TraceBasedUdpClient> udpClient = CreateObject<TraceBasedUdpClient>();
  udpClient->SetRemote(remoteAddr, 4000);
  udpClient->SetTraceFile(traceFile);
  udpClient->SetPacketSize(1472);
  nodes.Get(0)->AddApplication(udpClient);
  udpClient->SetStartTime(Seconds(2.0));
  udpClient->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}