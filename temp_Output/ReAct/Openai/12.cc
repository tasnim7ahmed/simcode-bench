#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaTraceUdpFlow");

class TraceBasedUdpClientApp : public Application
{
public:
  TraceBasedUdpClientApp ();
  virtual ~TraceBasedUdpClientApp ();

  void Setup (Ipv4Address address, uint16_t port, std::string traceFile, uint32_t packetSize, bool enableLog);
  void Setup (Ipv6Address address, uint16_t port, std::string traceFile, uint32_t packetSize, bool enableLog);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleNextSend ();
  void SendPacket ();

  Ptr<Socket>     m_socket;
  Address         m_peerAddress;
  std::vector<double> m_intervals;
  uint32_t        m_packetSize;
  uint32_t        m_currentIndex;
  EventId         m_sendEvent;
  std::string     m_traceFile;
  bool            m_running;
  bool            m_enableLog;
};

TraceBasedUdpClientApp::TraceBasedUdpClientApp ()
  : m_socket (0),
    m_packetSize (0),
    m_currentIndex (0),
    m_running (false),
    m_enableLog (false)
{
}

TraceBasedUdpClientApp::~TraceBasedUdpClientApp ()
{
  m_socket = 0;
}

void
TraceBasedUdpClientApp::Setup (Ipv4Address address, uint16_t port, std::string traceFile, uint32_t packetSize, bool enableLog)
{
  m_peerAddress = InetSocketAddress (address, port);
  m_traceFile = traceFile;
  m_packetSize = packetSize;
  m_enableLog = enableLog;

  // Load trace file as inter-arrival times (in seconds) per line
  m_intervals.clear ();
  std::ifstream infile (traceFile);
  if (!infile)
    {
      NS_FATAL_ERROR ("Unable to open trace file: " << traceFile);
    }
  double interval;
  while (infile >> interval)
    {
      m_intervals.push_back (interval);
    }
}

void
TraceBasedUdpClientApp::Setup (Ipv6Address address, uint16_t port, std::string traceFile, uint32_t packetSize, bool enableLog)
{
  m_peerAddress = Inet6SocketAddress (address, port);
  m_traceFile = traceFile;
  m_packetSize = packetSize;
  m_enableLog = enableLog;

  // Load trace file as inter-arrival times (in seconds) per line
  m_intervals.clear ();
  std::ifstream infile (traceFile);
  if (!infile)
    {
      NS_FATAL_ERROR ("Unable to open trace file: " << traceFile);
    }
  double interval;
  while (infile >> interval)
    {
      m_intervals.push_back (interval);
    }
}

void
TraceBasedUdpClientApp::StartApplication (void)
{
  m_running = true;
  m_currentIndex = 0;

  if (InetSocketAddress::IsMatchingType (m_peerAddress))
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Connect (m_peerAddress);
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Connect (m_peerAddress);
    }
  else
    {
      NS_FATAL_ERROR ("Unknown address type in TraceBasedUdpClientApp::StartApplication");
    }

  if (m_intervals.size () == 0)
    {
      NS_FATAL_ERROR ("No intervals loaded from trace file in TraceBasedUdpClientApp");
    }

  if (m_enableLog)
    {
      NS_LOG_INFO ("[UDP Client] Started at: " << Simulator::Now ().GetSeconds ()
                                              << "s, Packets to send: " << m_intervals.size ());
    }

  ScheduleNextSend ();
}

void
TraceBasedUdpClientApp::StopApplication (void)
{
  m_running = false;
  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
  if (m_enableLog)
    {
      NS_LOG_INFO ("[UDP Client] Stopping at: " << Simulator::Now ().GetSeconds () << "s");
    }
}

void
TraceBasedUdpClientApp::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (m_enableLog)
    {
      NS_LOG_INFO ("[UDP Client] Sent packet " << m_currentIndex + 1 << "/"
                    << m_intervals.size () << " at "
                    << Simulator::Now ().GetSeconds () << "s");
    }

  ++m_currentIndex;
  ScheduleNextSend ();
}

void
TraceBasedUdpClientApp::ScheduleNextSend ()
{
  if (!m_running || m_currentIndex >= m_intervals.size ())
    {
      return;
    }
  Time nextTime = Seconds (m_intervals[m_currentIndex]);
  m_sendEvent = Simulator::Schedule (nextTime, &TraceBasedUdpClientApp::SendPacket, this);
}

// --- Main program ---
int main (int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;
  std::string traceFile = "trace.txt";

  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Set to true to use IPv6 (default: false, uses IPv4)", useIpv6);
  cmd.AddValue ("enableLogging", "Enable detailed UDP client/server logging", enableLogging);
  cmd.AddValue ("traceFile", "Path to packet trace file with per-line inter-send intervals (seconds)", traceFile);
  cmd.Parse (argc, argv);

  uint32_t mtu = 1500;
  uint32_t udpIpHeader = 20 + 8;
  if (useIpv6)
    {
      udpIpHeader = 40 + 8;
    }
  uint32_t packetSize = mtu - udpIpHeader; // 1472 for IPv4, 1452 for IPv6

  if (enableLogging)
    {
      LogComponentEnable ("CsmaTraceUdpFlow", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (mtu));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4Address n0Ipv4;
  Ipv4Address n1Ipv4;
  Ipv6Address n0Ipv6;
  Ipv6Address n1Ipv6;

  Ipv4InterfaceContainer ipv4Ifaces;
  Ipv6InterfaceContainer ipv6Ifaces;

  if (!useIpv6)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      ipv4Ifaces = ipv4.Assign (devices);
      n0Ipv4 = ipv4Ifaces.GetAddress (0);
      n1Ipv4 = ipv4Ifaces.GetAddress (1);
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:1::", 64);
      ipv6Ifaces = ipv6.Assign (devices);
      n0Ipv6 = ipv6Ifaces.GetAddress (0, 1);
      n1Ipv6 = ipv6Ifaces.GetAddress (1, 1);
    }

  uint16_t port = 4000;

  // UDP server on node 1
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Client on node 0, uses custom application
  Ptr<TraceBasedUdpClientApp> clientApp = CreateObject<TraceBasedUdpClientApp> ();
  if (!useIpv6)
    {
      clientApp->Setup (n1Ipv4, port, traceFile, packetSize, enableLogging);
    }
  else
    {
      clientApp->Setup (n1Ipv6, port, traceFile, packetSize, enableLogging);
    }
  nodes.Get(0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (2.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}