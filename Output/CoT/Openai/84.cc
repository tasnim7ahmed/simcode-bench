#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaLanUdpTraceFlow");

struct TracePacket
{
  uint32_t size;
  double time; // seconds (relative to start)
};

std::vector<TracePacket>
ReadTraceFile(const std::string &filename)
{
  std::vector<TracePacket> tracePackets;
  std::ifstream infile(filename);
  std::string line;
  while (std::getline(infile, line))
    {
      std::istringstream iss(line);
      double time;
      uint32_t size;
      if (!(iss >> time >> size))
        continue;
      TracePacket pkt;
      pkt.time = time;
      pkt.size = size;
      tracePackets.push_back(pkt);
    }
  return tracePackets;
}

class UdpTraceClientApp : public Application
{
public:
  UdpTraceClientApp();
  virtual ~UdpTraceClientApp();

  void Setup(Address address, uint16_t port, const std::vector<TracePacket> &tracePackets);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleNextPacket();
  void SendPacket();

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint16_t        m_peerPort;
  std::vector<TracePacket> m_tracePackets;
  uint32_t        m_currentIndex;
  EventId         m_sendEvent;
  bool            m_running;
  Time            m_startTime;
};

UdpTraceClientApp::UdpTraceClientApp()
    : m_socket(0),
      m_peer(),
      m_peerPort(0),
      m_currentIndex(0),
      m_sendEvent(),
      m_running(false)
{
}

UdpTraceClientApp::~UdpTraceClientApp()
{
  m_socket = 0;
}

void UdpTraceClientApp::Setup(Address address, uint16_t port, const std::vector<TracePacket> &tracePackets)
{
  m_peer = address;
  m_peerPort = port;
  m_tracePackets = tracePackets;
}

void UdpTraceClientApp::StartApplication()
{
  m_running = true;
  m_currentIndex = 0;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(InetSocketAddress::IsMatchingType(m_peer) ? m_peer : Inet6SocketAddress(m_peer));
  m_startTime = Simulator::Now();
  if (!m_tracePackets.empty())
    {
      ScheduleNextPacket();
    }
}

void UdpTraceClientApp::StopApplication()
{
  m_running = false;
  if (m_sendEvent.IsRunning())
    Simulator::Cancel(m_sendEvent);
  if (m_socket)
    m_socket->Close();
}

void UdpTraceClientApp::ScheduleNextPacket()
{
  if (m_currentIndex >= m_tracePackets.size())
    return;

  Time now = Simulator::Now();
  double absTrigTime = m_tracePackets[m_currentIndex].time;
  Time nextTime = Seconds(absTrigTime) - (now - m_startTime);
  if (nextTime.IsNegative() || nextTime.IsZero())
    nextTime = MicroSeconds(1); // As soon as possible
  m_sendEvent = Simulator::Schedule(nextTime, &UdpTraceClientApp::SendPacket, this);
}

void UdpTraceClientApp::SendPacket()
{
  if (!m_running || m_currentIndex >= m_tracePackets.size())
    return;

  uint32_t pktSize = m_tracePackets[m_currentIndex].size;
  Ptr<Packet> pkt = Create<Packet>(pktSize);
  m_socket->Send(pkt);
  ++m_currentIndex;
  if (m_currentIndex < m_tracePackets.size())
    {
      ScheduleNextPacket();
    }
}

int main(int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;
  std::string traceFile = "udp-trace.txt";

  CommandLine cmd;
  cmd.AddValue("ipv6", "Use IPv6 instead of IPv4", useIpv6);
  cmd.AddValue("log", "Enable info logging", enableLogging);
  cmd.AddValue("traceFile", "Path to UDP packet trace file: each line is <time> <size>", traceFile);
  cmd.Parse(argc, argv);

  if (enableLogging)
    {
      LogComponentEnable("CsmaLanUdpTraceFlow", LOG_LEVEL_INFO);
      LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
      LogComponentEnable("UdpTraceClientApp", LOG_LEVEL_INFO);
    }

  std::vector<TracePacket> tracePackets = ReadTraceFile(traceFile);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("2ms"));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4Address serverIpv4Addr;
  Ipv6Address serverIpv6Addr;

  uint16_t port = 50000;

  if (!useIpv6)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);
      serverIpv4Addr = ifaces.GetAddress(1);
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
      Ipv6InterfaceContainer ifaces = ipv6.Assign(devices);
      ifaces.SetForwarding(0, true);
      ifaces.SetDefaultRouteInAllNodes(0);
      serverIpv6Addr = ifaces.GetAddress(1, 1); // index 1 = global address
    }

  // UDP server (PacketSink)
  ApplicationContainer serverApp;
  if (!useIpv6)
    {
      Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
      serverApp = PacketSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress).Install(nodes.Get(1));
    }
  else
    {
      Address sinkLocalAddress(Inet6SocketAddress(Ipv6Address::GetAny(), port));
      serverApp = PacketSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress).Install(nodes.Get(1));
    }
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // UDP trace-driven client
  Ptr<UdpTraceClientApp> clientApp = CreateObject<UdpTraceClientApp>();
  Address serverAddress;
  if (!useIpv6)
    {
      serverAddress = InetSocketAddress(serverIpv4Addr, port);
    }
  else
    {
      serverAddress = Inet6SocketAddress(serverIpv6Addr, port);
    }
  clientApp->Setup(serverAddress, port, tracePackets);
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(2.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}