#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <stdint.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceApp");

class UdpTraceClient : public Application
{
public:
  UdpTraceClient();
  virtual ~UdpTraceClient();
  void SetRemote(Address remote, uint16_t port);
  void SetTraceFile(const std::string &traceFileName);
  void SetPacketSize(uint32_t size);

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void ScheduleNextPacket();
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  std::vector<double> m_times;
  uint32_t m_traceIndex;
  EventId m_sendEvent;
  bool m_running;
  std::string m_traceFileName;
  uint32_t m_packetSize;
};

UdpTraceClient::UdpTraceClient()
    : m_socket(0),
      m_peerPort(0),
      m_traceIndex(0),
      m_running(false),
      m_packetSize(1472)
{
}

UdpTraceClient::~UdpTraceClient()
{
  m_socket = 0;
}

void UdpTraceClient::SetRemote(Address remote, uint16_t port)
{
  m_peerAddress = remote;
  m_peerPort = port;
}

void UdpTraceClient::SetTraceFile(const std::string &traceFileName)
{
  m_traceFileName = traceFileName;
}

void UdpTraceClient::SetPacketSize(uint32_t size)
{
  m_packetSize = size;
}

void UdpTraceClient::StartApplication()
{
  m_running = true;
  m_traceIndex = 0;
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Connect(InetSocketAddress::ConvertFrom(m_peerAddress));
    }

  // Read trace file
  m_times.clear();
  std::ifstream traceFile(m_traceFileName.c_str());
  std::string line;
  while (std::getline(traceFile, line))
    {
      std::istringstream iss(line);
      double time = 0.0;
      if (!(iss >> time))
        continue;
      m_times.push_back(time);
    }
  traceFile.close();

  if (m_times.empty())
    return;

  // Schedule first send
  Time next = Seconds(m_times[0]);
  m_sendEvent = Simulator::Schedule(next - Now(), &UdpTraceClient::SendPacket, this);
}

void UdpTraceClient::StopApplication()
{
  m_running = false;
  if (m_sendEvent.IsRunning())
    Simulator::Cancel(m_sendEvent);
  if (m_socket)
    m_socket->Close();
}

void UdpTraceClient::ScheduleNextPacket()
{
  ++m_traceIndex;
  if (m_traceIndex < m_times.size())
    {
      Time nextTime = Seconds(m_times[m_traceIndex]);
      Time now = Simulator::Now();
      Time delay = nextTime - now;
      if (delay.IsNegative())
        delay = Seconds(0);
      m_sendEvent = Simulator::Schedule(delay, &UdpTraceClient::SendPacket, this);
    }
}

void UdpTraceClient::SendPacket()
{
  NS_LOG_INFO("UdpTraceClient sent packet at " << Simulator::Now().GetSeconds() << "s");
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  ScheduleNextPacket();
}

int main(int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;
  std::string traceFile = "trace.txt";

  CommandLine cmd;
  cmd.AddValue("ipv6", "Enable IPv6 (default: false)", useIpv6);
  cmd.AddValue("logging", "Enable detailed logging", enableLogging);
  cmd.AddValue("traceFile", "Trace file for client sending times", traceFile);
  cmd.Parse(argc, argv);

  if (enableLogging)
    {
      LogComponentEnable("CsmaUdpTraceApp", LOG_LEVEL_INFO);
      LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable("UdpTraceClient", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  if (useIpv6)
    stack.SetIpv4StackInstall(false);
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer interfaces4;
  Ipv6InterfaceContainer interfaces6;
  if (!useIpv6)
    {
      ipv4.SetBase("10.1.1.0", "255.255.255.0");
      interfaces4 = ipv4.Assign(devices);
    }
  else
    {
      ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
      interfaces6 = ipv6.Assign(devices);
    }

  uint16_t serverPort = 4000;
  double serverStart = 1.0;
  double serverStop = 10.0;
  double clientStart = 2.0;
  double clientStop = 10.0;

  UdpServerHelper server(serverPort);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(serverStart));
  serverApps.Stop(Seconds(serverStop));

  Ptr<UdpTraceClient> clientApp = CreateObject<UdpTraceClient>();
  clientApp->SetAttribute("StartTime", TimeValue(Seconds(clientStart)));
  clientApp->SetAttribute("StopTime", TimeValue(Seconds(clientStop)));

  Address clientAddr;
  if (!useIpv6)
    {
      clientAddr = InetSocketAddress(interfaces4.GetAddress(1), serverPort);
    }
  else
    {
      clientAddr = Inet6SocketAddress(interfaces6.GetAddress(1,1), serverPort);
    }
  clientApp->SetRemote(clientAddr, serverPort);
  clientApp->SetTraceFile(traceFile);
  clientApp->SetPacketSize(1472);

  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(clientStart));
  clientApp->SetStopTime(Seconds(clientStop));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}