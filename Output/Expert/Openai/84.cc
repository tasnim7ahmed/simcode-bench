#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceApp");

class TraceBasedUdpClientApp : public Application
{
public:
  TraceBasedUdpClientApp();
  virtual ~TraceBasedUdpClientApp();
  void Setup(Address address, uint16_t port, std::string traceFile);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void ScheduleNextPacket();
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  std::ifstream m_traceStream;
  std::string m_traceFileName;
  EventId m_sendEvent;
  bool m_running;
  std::vector<uint32_t> m_packetSizes;
  uint32_t m_currentPacket;
};

TraceBasedUdpClientApp::TraceBasedUdpClientApp()
    : m_socket(0),
      m_peerPort(0),
      m_sendEvent(),
      m_running(false),
      m_currentPacket(0)
{
}

TraceBasedUdpClientApp::~TraceBasedUdpClientApp()
{
  m_traceStream.close();
  m_socket = 0;
}

void
TraceBasedUdpClientApp::Setup(Address address, uint16_t port, std::string traceFile)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_traceFileName = traceFile;
}

void
TraceBasedUdpClientApp::StartApplication()
{
  m_running = true;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(InetSocketAddress::ConvertFrom(m_peerAddress));
  m_traceStream.open(m_traceFileName, std::ios::in);

  m_packetSizes.clear();
  std::string line;
  while (std::getline(m_traceStream, line))
    {
      uint32_t pktSize = std::stoul(line);
      m_packetSizes.push_back(pktSize);
    }
  m_currentPacket = 0;
  m_traceStream.close();

  if (!m_packetSizes.empty())
    {
      SendPacket();
    }
}

void
TraceBasedUdpClientApp::StopApplication()
{
  m_running = false;

  if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }
  if (m_socket)
    {
      m_socket->Close();
    }
}

void
TraceBasedUdpClientApp::SendPacket()
{
  if (!m_running || m_currentPacket >= m_packetSizes.size())
    {
      return;
    }

  uint32_t size = m_packetSizes[m_currentPacket];
  Ptr<Packet> packet = Create<Packet>(size);
  m_socket->Send(packet);

  ++m_currentPacket;
  if (m_currentPacket < m_packetSizes.size())
    {
      // Send next packet immediately upon previous send (no time encoded in trace)
      m_sendEvent = Simulator::Schedule(Seconds(0.0), &TraceBasedUdpClientApp::SendPacket, this);
    }
}

int main(int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;
  std::string traceFilename = "trace.txt";

  CommandLine cmd;
  cmd.AddValue("useIpv6", "Enable IPv6 addressing", useIpv6);
  cmd.AddValue("enableLogging", "Enable logging", enableLogging);
  cmd.AddValue("traceFile", "UDP packet size trace file", traceFilename);
  cmd.Parse(argc, argv);

  if (enableLogging)
    {
      LogComponentEnable("CsmaUdpTraceApp", LOG_LEVEL_INFO);
      LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  if (useIpv6)
    {
      stack.SetIpv4StackInstall(false);
      stack.SetIpv6StackInstall(true);
    }
  else
    {
      stack.SetIpv4StackInstall(true);
      stack.SetIpv6StackInstall(false);
    }
  stack.Install(nodes);

  Ipv4Address serverIpv4;
  Ipv6Address serverIpv6;
  Ipv4InterfaceContainer ipv4Intf;
  Ipv6InterfaceContainer ipv6Intf;

  if (useIpv6)
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
      ipv6Intf = ipv6.Assign(devices);
      serverIpv6 = ipv6Intf.GetAddress(1, 1);
    }
  else
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase("10.1.1.0", "255.255.255.0");
      ipv4Intf = ipv4.Assign(devices);
      serverIpv4 = ipv4Intf.GetAddress(1);
    }

  uint16_t serverPort = 9000;

  ApplicationContainer serverApp;
  if (useIpv6)
    {
      UdpServerHelper server(serverPort);
      serverApp = server.Install(nodes.Get(1));
    }
  else
    {
      UdpServerHelper server(serverPort);
      serverApp = server.Install(nodes.Get(1));
    }
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Setup and install UDP client with trace
  Ptr<TraceBasedUdpClientApp> traceClient = CreateObject<TraceBasedUdpClientApp>();
  if (useIpv6)
    {
      Address serverAddr = Inet6SocketAddress(serverIpv6, serverPort);
      traceClient->Setup(serverAddr, serverPort, traceFilename);
    }
  else
    {
      Address serverAddr = InetSocketAddress(serverIpv4, serverPort);
      traceClient->Setup(serverAddr, serverPort, traceFilename);
    }
  nodes.Get(0)->AddApplication(traceClient);
  traceClient->SetStartTime(Seconds(2.0));
  traceClient->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}