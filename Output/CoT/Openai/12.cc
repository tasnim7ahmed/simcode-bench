#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceFileExample");

class TraceFileHelper : public Object
{
public:
  TraceFileHelper() {}
  void Load(const std::string &traceFile)
  {
    std::ifstream infile(traceFile);
    if (!infile)
    {
      NS_FATAL_ERROR("Unable to open trace file: " << traceFile);
    }
    double time;
    uint32_t size;
    while (infile >> time >> size)
    {
      m_times.push_back(time);
      m_sizes.push_back(size);
    }
  }
  uint32_t GetEventCount() const
  {
    return m_times.size();
  }
  double GetTime(uint32_t idx) const
  {
    return m_times[idx];
  }
  uint32_t GetSize(uint32_t idx) const
  {
    return m_sizes[idx];
  }
private:
  std::vector<double> m_times;
  std::vector<uint32_t> m_sizes;
};

class TraceDrivenUdpClient : public Application
{
public:
  TraceDrivenUdpClient() : m_socket(0), m_seq(0) {}
  void Setup(Address address, uint16_t port, Ptr<TraceFileHelper> traceHelper, uint32_t maxPacketSize)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_traceHelper = traceHelper;
    m_maxPacketSize = maxPacketSize;
  }
protected:
  virtual void StartApplication(void)
  {
    if (InetSocketAddress::IsMatchingType(m_peerAddress))
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    }
    else
    {
      NS_FATAL_ERROR("Unknown peer address type");
    }
    m_socket->Connect(m_peerAddress);

    ScheduleNextPacket();
  }
  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
    Simulator::Cancel(m_event);
  }
  void ScheduleNextPacket()
  {
    if (m_seq < m_traceHelper->GetEventCount())
    {
      double now = Simulator::Now().GetSeconds();
      double pktTime = m_traceHelper->GetTime(m_seq);
      double delay = pktTime > now ? pktTime - now : 0.0;
      m_event = Simulator::Schedule(Seconds(delay), &TraceDrivenUdpClient::SendPacket, this);
    }
  }
  void SendPacket()
  {
    if (m_seq < m_traceHelper->GetEventCount())
    {
      uint32_t size = m_traceHelper->GetSize(m_seq);
      if (size > m_maxPacketSize)
      {
        size = m_maxPacketSize;
      }
      Ptr<Packet> packet = Create<Packet>(size);
      m_socket->Send(packet);
      NS_LOG_INFO("Client sent packet #" << m_seq << " size=" << size << " at " << Simulator::Now().GetSeconds());
      ++m_seq;
      ScheduleNextPacket();
    }
  }
private:
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  EventId m_event;
  Ptr<TraceFileHelper> m_traceHelper;
  uint32_t m_seq;
  uint32_t m_maxPacketSize;
};

void
UdpServerRxCallback(Ptr<const Packet> packet, const Address &srcAddress)
{
  NS_LOG_INFO("Server received packet of size " << packet->GetSize() << " from " << InetSocketAddress::ConvertFrom(srcAddress).GetIpv4() << " at " << Simulator::Now().GetSeconds());
}

void
UdpServer6RxCallback(Ptr<const Packet> packet, const Address &srcAddress)
{
  NS_LOG_INFO("Server received packet of size " << packet->GetSize() << " from " << Inet6SocketAddress::ConvertFrom(srcAddress).GetIpv6() << " at " << Simulator::Now().GetSeconds());
}

int main(int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;
  std::string traceFile = "trace.txt";

  CommandLine cmd;
  cmd.AddValue("useIpv6", "Use IPv6 addressing (default: false)", useIpv6);
  cmd.AddValue("enableLogging", "Enable logging (default: false)", enableLogging);
  cmd.AddValue("traceFile", "Trace file with packet (time size) per line", traceFile);
  cmd.Parse(argc, argv);

  if (enableLogging)
  {
    LogComponentEnable("CsmaUdpTraceFileExample", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  }

  // Load trace file
  Ptr<TraceFileHelper> traceHelper = CreateObject<TraceFileHelper>();
  traceHelper->Load(traceFile);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4Address serverIpv4;
  Ipv6Address serverIpv6;

  uint16_t port = 4000;

  if (!useIpv6)
  {
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);
    serverIpv4 = i.GetAddress(1);
  }
  else
  {
    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:db8::", 64);
    Ipv6InterfaceContainer i6 = ipv6.Assign(devices);
    i6.SetForwarding(0, true);
    i6.SetForwarding(1, true);
    i6.SetDefaultRouteInAllNodes(0);
    serverIpv6 = i6.GetAddress(1, 1);
  }

  ApplicationContainer serverApps;
  Ptr<UdpServer> udpServer;
  Ptr<Application> serverApp;
  if (!useIpv6)
  {
    udpServer = CreateObject<UdpServer>();
    udpServer->SetAttribute("Port", UintegerValue(port));
    nodes.Get(1)->AddApplication(udpServer);
    udpServer->SetStartTime(Seconds(1.0));
    udpServer->SetStopTime(Seconds(10.0));
    if (enableLogging)
    {
      udpServer->TraceConnectWithoutContext("Rx", MakeCallback(&UdpServerRxCallback));
    }
  }
  else
  {
    Ptr<UdpServer> udpServer6 = CreateObject<UdpServer>();
    udpServer6->SetAttribute("Port", UintegerValue(port));
    nodes.Get(1)->AddApplication(udpServer6);
    udpServer6->SetStartTime(Seconds(1.0));
    udpServer6->SetStopTime(Seconds(10.0));
    if (enableLogging)
    {
      udpServer6->TraceConnectWithoutContext("Rx", MakeCallback(&UdpServer6RxCallback));
    }
  }

  // Client setup
  Ptr<TraceDrivenUdpClient> client = CreateObject<TraceDrivenUdpClient>();
  Address dstAddr;
  if (!useIpv6)
  {
    dstAddr = InetSocketAddress(serverIpv4, port);
  }
  else
  {
    dstAddr = Inet6SocketAddress(serverIpv6, port);
  }
  uint32_t mtu = 1500;
  uint32_t maxPktSize = mtu - 20 - 8; // 1472 bytes
  client->Setup(dstAddr, port, traceHelper, maxPktSize);
  nodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(2.0));
  client->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}